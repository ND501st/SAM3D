# 10. Bodies and voices

Roadmap 8.10: the crewmates are seen working their controls and heard. Pillar 3 says knowledge moves only through
instruments and voice; this is the part of it a player sees and hears. Built 17 October 2026.

## 1. What the player sees

Each NPC crewmate is a body in his seat in the 3D cabin:

- **His hand goes to the control he is about to move.** The hand sets off 0.9 s before the action (the longest move
  takes 0.8 s), so it arrives when the switch moves. It stays there a moment and then comes back. A toggle, button or cover gets a finger;
  a knob, key or hand wheel gets a grip.
- **He grips the hand wheel he is turning** while he tracks by hand. The hand rides round with the rim and takes a new
  grip every 120 degrees.
- **He bends, twists and turns on his seat** to reach a control. For a control he can't reach sitting down, he stands
  up and steps to it: the deputy commander at rack SŁ-1M for the orientation, the commander at the right R-123M on the
  floor behind his seat.
- **He looks at what he works.** Otherwise he watches his instrument: the senior operator the search scope, the
  operator the range indicator, the deputy commander the TV monitor. The driver and the commander look out. In the
  PU-12M the battery commander watches the IT-45 and the operator-plotter his map (a seat's `look` can name alternatives,
  `"ssc_range|map"`, the first one the cabin has: one `operator` entry serves both vehicles).
- **He nods while he speaks.** He breathes.
- **The driver's hands rest on the steering wheel** and his feet on the pedals.

The body at the camera's seat isn't drawn (first person). The training overlay's **ghost hand** pulses on the
control the drill master advises (`08`).

## 2. The model (simcore, `crew_body.hpp`)

| Piece | What it does |
|---|---|
| `BodyData` (`data/crew/body.json`) | The crewman's stature (1.72 m, an estimate); joint heights and segment lengths as fractions of it (`winter_biomech`); seat height 430 mm (`gost_12_2_032_78` Table 2); head turn up to 60° (GOST §4.4); bend, twist, seat turn and crouch limits (estimates); each seat's gaze and rest posture |
| `SeatFrameOf` | The hip joints and the floor from a station's eye point and heading. The sitting eye height (0.79 m) and the seat height put the floor where the cabin has it (0.62 m), to a centimetre |
| `TwoBoneIk` | The elbow (knee) of a two-segment chain, bent toward a pole |
| `TouchOf` | Where a hand meets a control: a finger's pad 12 mm out from a toggle; the palm on a knob, a key or a hand wheel's rim |
| `PlanReach` | The cheapest posture from which a hand meets the control. Sitting, it searches over a turn on the seat, the bend and the twist. If none works, he stands at the panel and may bend or crouch. Given the cabin, it keeps his head and chest out of the equipment behind every panel (a 20 cm slab behind its face) and his head under the roof (the cabin's `ceiling_m`); standing, his knees and shins too, stepping back up to 24 cm from the panel when a crouch needs the room (`ReachSpace`). Unreachable controls are reported |
| `SolveBody` | The pose: pelvis, torso, head with its gaze, both arms, legs (thighs on the seat, feet on the floor or the pedals; standing, the feet under the hips, the knees bent in a crouch) |
| `PartsOf` | The pose as 16 rigid parts (origin, bone, up, length) for a renderer |
| `BodyAnimator` | A seat's body over time from `SeatActivity` (§3): hand moves along a short arc at 1.2 m/s, the lean smoothed, standing over a second, wheel grips |

**The reach check.** It is a test (`test_crew_body.cpp`): every control of the Osa cabin must be reachable from its
station, and a control that needs standing must be in the crew data's `stand_for` list, and only those. It found
nine:

- the deputy commander's four orientation controls, 1.4–1.6 m along the wall;
- the commander's right-radio controls, low behind his seat.

In the PU-12M two: the squad leader's DTP, low at his side, and the radio operator's mast winch. The roof is 1.40 m over
the floor there, so both crouch; the PU-12M test checks that every reach keeps the head under the roof and out of the
equipment with the finger on the control.

**Drill time.** Standing costs time in the drill: `CrewMember` adds 1.5 s to get up and step, and 1.0 s to sit back
down (crew data `standing`). The drill master's ideal crew pays it too.

## 3. What each seat is doing (`ScenarioRunner::CrewActivity`)

One `SeatActivity` per crew member, built from `CrewMember::CurrentIntent` and the bus:

- the next action's control and the seconds until it (`act_in_s`; < 0 while he waits for a condition);
- the control he moved last, and how long ago;
- whether he is up at a panel, and for which control;
- the hand wheels he is turning: any wheel of his station that moved in the last half second;
- whether he is speaking (`CalloutBus::SpeakingFrom`), and whether he is stressed.

## 4. Voices

| Piece | What it does |
|---|---|
| `data/crew/voices.json` | One file for every crew (the Osa's and the PU-12M's; a crew takes its own seats' voices). The languages in order (Russian, then English); each seat's voice (pitch and rate on the speech engine's −10..10 scale); stress (+2 pitch, +2 rate); the intercom's band (300–3400 Hz) and the radio's (300–3000 Hz); how the crew says its abbreviations (ГТА "гэ тэ а", ССЦ "эс эс цэ", СОЦ as a word) |
| `crew_voice.hpp` | `SpeechText`: a callout's words for a speech engine. Azimuths in mils are read in pairs ("35-00": "тридцать пять ноль-ноль", "three five, zero zero"), board numbers in pairs, other numbers in Russian words, abbreviations spelled. `VoiceFor`: a speaker's voice, stressed or not, intercom or radio |
| `CalloutBus` | Numbers each line as it starts (`seq`) and says what is being said now (`Now`), so a voice plays each line once |
| Unreal `FSimSpeech` | Windows' own speech engine (SAPI) on a worker thread, with its desktop voices and the "OneCore" voices of the language packs. Text in, 16-bit PCM out |
| Unreal `USimCrewVoiceComponent` | Plays every new line in its seat's voice through the intercom's or the radio's band. Radio lines get a little hiss and the squelch's tail. A new line on a channel keys over the old one |

**Languages.** The crew speaks Russian when a Russian voice is installed. On Windows that means Settings, then Time &
language, then Speech, then Add voices, then Russian. Otherwise it speaks the English lines. The machine this was
built on has only English voices (David, Mark, Zira), so the crew there speaks English.

**Recorded lines.** A recorded line takes precedence for any fixed callout (one with no filled-in words):
`art/crew_voices/<language>/<seat>/<callout>.wav`, or `art/crew_voices/<language>/<callout>.wav`, 16-bit PCM. Real
voice actors can replace the engine line by line.

**Timing.** The simulation keeps its own timing: a line takes 0.4 s plus 0.35 s a word on the bus, whatever the voice
does. The seats' rates are a little fast to stay close to that. Above 1.5× time compression the crew isn't voiced.
Console: `sam3d.Voice`, `sam3d.VoiceLanguage auto|ru|en`, `sam3d.VoiceVolume`.

## 5. Art and tools

| Tool | Does |
|---|---|
| `tools/blender/build_crew_body.py` | Builds the parts from `body.json`'s sizes: torso with belt, a padded tank helmet (шлемофон) with its ribs, earphone cups and chin strap, the face, arms, three hand shapes per side (rest, point, grip), legs with tall boots. Writes `art/crew_body/*.fbx` and `crew_body.blend` |
| `tools/ue_import_crew_body.py` | Imports them as `/Game/SAM3D/Crew/SM_Crew_*` (editor closed, through the commandlet) |
| `simcore/examples/crew_pose_dump` + `tools/blender/render_crew_poses.py` | Pose every seat at rest and at work (16 poses; `crew_pose_dump data <out> pu12m`: the PU-12M's 14) and render them in the furnished cabin, with a red bead on the control being reached: the check by eye |
| `tools/blender/pu12m_interior_check.py -- --poses <poses>` | The PU-12M's posed bodies against its meshes: what goes into what, and how deep (`docs/PU12_DEVELOPMENT.md`) |

The parts are rigid: each has its origin at its proximal joint, its bone along +X and its "up" along +Z. Engine
basic shapes stand in for any part that isn't imported.

## 6. Checks

- **simcore tests:**
  - the data loads;
  - the IK against a hand calculation;
  - the seats put the floor where the cabin has it;
  - every control is in reach of its seat, standing only for `stand_for` (the Osa and the PU-12M);
  - a ball behind a panel's face is in its equipment, one in front or past its edge clear (`ReachSpace`, a hand
    calculation);
  - in the PU-12M, every reach keeps the head under the roof and out of the equipment, the finger on the control;
  - the finger is on the switch when it moves and the hand comes back;
  - the deputy stands at the gyro panel;
  - a wheel grip rides round with the wheel;
  - parts keep their lengths;
  - a crewman pays the time to stand and sit;
  - the runner's activity feed;
  - the voice's words, numbers and abbreviations;
  - every seat has a voice;
  - the bus's "now".
- **Unreal automation, headless:**
  - `SAM3D.Crew.Speech`: the engine speaks a contact report, 4.2 s, not silence;
  - `SAM3D.Crew.Bodies`: five bodies in a game world, each head 10 cm from its station's eye point, the camera's seat
    hidden.

## 7. Not yet

- Skinned meshes and hand-made animations: the rigid parts are placeholders, posed by the same simcore model a
  skeletal mesh would use.
- Lips.
- Players' bodies in multiplayer: another player's seat shows his body at rest.
- Voices on a network client (the client mirrors the console, not the bus).
- Russian voices on this machine.
