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
| `data/crew/voices.json` | One file for every crew (the Osa's and the PU-12M's; a crew takes its own seats' voices). The languages in order (Russian, then English); each seat's voice (pitch and rate on the speech engine's −10..10 scale); stress (at full stress +2 pitch, +2 rate and 30 % louder, in proportion below it: `15_TALK.md` §4); what the abbreviations mean (`gloss`, shown with the helper on: `15_TALK.md` §6); the intercom's band (300–3400 Hz) and the radio's (300–3000 Hz); how the crew says its abbreviations (ГТА "гэ тэ а", ССЦ "эс эс цэ", СОЦ as a word) |
| `crew_voice.hpp` | `SpeechText`: a callout's words for a speech engine. Azimuths in mils are read in pairs ("35-00": "тридцать пять ноль-ноль", "three five, zero zero"), board numbers in pairs, other numbers in Russian words, abbreviations spelled. `VoiceFor`: a speaker's voice, stressed or not, intercom or radio |
| `CalloutBus` | Numbers each line as it starts (`seq`) and says what is being said now (`Now`), so a voice plays each line once |
| Unreal `FSimSpeech` | Windows' own speech engine (SAPI) on a worker thread, with its desktop voices and the "OneCore" voices of the language packs. Text in, 16-bit PCM out |
| Unreal `USimCrewVoiceComponent` | Plays every new line in its seat's voice through the intercom's or the radio's band. Intercom lines first go through a throat microphone (the R-124's laryngophone headsets: `15_TALK.md` §7); a word said aloud in the cabin comes from the speaker's head; a channel with a less urgent line is ducked under a more urgent one. Radio lines get a little hiss and the squelch's tail. A new line on a channel keys over the old one |

**Languages.** The crew speaks Russian when a Russian voice is installed. On Windows that means Settings, then Time &
language, then Speech, then Add voices, then Russian. Otherwise it speaks the English lines. The machine this was
built on has only English voices (David, Mark, Zira), so the crew there speaks English.

**Recorded lines.** A recorded line takes precedence for any fixed callout (one with no filled-in words):
`art/crew_voices/<language>/<seat>/<callout>.wav`, or `art/crew_voices/<language>/<callout>.wav`, 16-bit PCM. Real
voice actors can replace the engine line by line.

The English fixed lines are recorded (27 Oct 2026; Russian later) by `tools/voice_lines.py` with Kokoro-82M, a free
offline speech model (Apache-2.0), in a voice per seat from `data/crew/recorded_voices.json`:

| Seat | Voice | | Seat | Voice |
|---|---|---|---|---|
| commander | am_echo | | battery_commander | am_adam |
| deputy_commander | am_michael | | squad_leader | am_liam |
| senior_operator | am_puck | | radio_operator | am_fenrir |
| operator (both vehicles) | am_eric | | driver (both vehicles) | am_onyx |

The voices were picked by measured pitch, pace and brightness so that the voices heard together (a crew's seats, and
the two commanders on the battery net) are as far apart as possible, then approved by ear.

- **What is recorded.** Every callout of the two crews whose words have nothing filled in, in the voice of each seat
  that says it ("any" is every seat of that crew), 175 files. The words are the ones the game would give the speech
  engine (`SpeechText`, ported: mils in digit pairs, `voices.json`'s `say` table: ARM is "A R M").
- **Not recorded.**
  - The driver's `power_on` is "Power on!" in the Osa and "Generator running, power on!" in the PU-12M. One file
    can't be both, so it isn't recorded and both crews' engines say it. Recording it needs a folder per crew, a
    change to `PlayRecorded`.
  - "Air!" (`air_alert`) for the commander, the driver and the operator. Kokoro puts a consonant before the lone
    vowel in these voices ("Where?", "There.", "Lear?") in every take tried, over 300 (phonemes, style lengths,
    speeds, cutting the start). A retake with `"record": false` leaves it to the game's speech engine. The deputy
    commander's and the senior operator's "Air!" are recorded; the senior operator's is heard as "Aaron." without the
    hint, so listen to it.
- **Each file.** Silence trimmed (30 ms before, 120 ms after), the speech at −18 dBFS RMS with peaks under −1 dBFS,
  16-bit mono at 24 kHz. `art/crew_voices/en/manifest.json` keeps each line's words, voice, settings, the phonemes
  the engine was given, its length and its checks. A rerun records only lines whose words, voice, settings or model
  changed, and removes files no line wants.
- **The checks.**
  - *empty*: less than 0.25 s of speech.
  - *clipped*: a flattened peak.
  - *words*: Whisper (`faster-whisper`, base.en) transcribes the file twice, plain and with the crew's jargon as a
    hint, and the closer transcript is compared with the words (word error over 0.25 fails). Listen to what it flags:
    of the first 14 flags, the owner heard 9 said wrong and 5 said right. A line heard right goes in
    `recorded_voices.json`'s `heard_right` with the transcript it was flagged for, and counts as *heard right* until a
    new recording of it is heard differently. A line heard wrong gets a retake there (`retakes`, with why): its own
    `speed`, the `phonemes` to give the engine in place of the words, or `style_length`, the voice's style for a
    longer input (Kokoro styles a voice by the input's length, and a short line can come out wrong in the short
    style). Retakes are found by trying the ways and keeping one Whisper hears right with and without the hint, as
    saved to 16 bits, and preferably one whose neighbours (the next style length, speed or spelling) pass too: a
    take that passes alone is a fluke of the transcriber. Six lines have retakes (27 Oct 2026).
    Two wholesale fixes were tried and dropped: misaki's phoneme form (the one Kokoro was trained on) gave 26 misheard
    lines against espeak's 20, and saying a line twice to keep the second gave worse lines.
  - *long*: longer than the speaking time of the line's Russian (0.4 s plus 0.35 s a word). In the game the bus gives
    a recorded line its recording's time (Timing, below), so nothing is cut off; *long* says the line slows the crew
    against a headless run. Reported, not a failure: 113 of the 175 are long.

  The first three fail the run (exit 1). `out/voice_gen/review.html` lists every line with a player, what Whisper
  heard, the phonemes and the checks, failures first, with a "said right" tick that writes the `heard_right` entry.
  Serve the repo root to play it (`.claude/launch.json`'s `voice-lines`, then
  `http://localhost:8766/out/voice_gen/review.html`).
- **Running it.** The audio libraries live in their own environment, out of the repo:

  ```
  python -m venv out/voice_gen/.venv
  out/voice_gen/.venv/Scripts/python -m pip install kokoro-onnx==0.6.1 faster-whisper==1.2.1 soundfile
  ```

  Put Kokoro's `kokoro-v1.0.onnx` (325 MB) and `voices-v1.0.bin` (28 MB) from the kokoro-onnx releases
  (`model-files-v1.0`) in `out/voice_gen/models/`. Fetch Whisper with
  `faster_whisper.download_model("base.en", output_dir="out/voice_gen/models/whisper-base.en")` (141 MB). Then:
  `out/voice_gen/.venv/Scripts/python -I tools/voice_lines.py` (`--force` to record everything again, `--only
  seat/callout` for some lines, `--list` to see the lines). `python tools/voice_lines.py --check` needs no audio
  libraries and exits 1 when the recordings no longer match the crews' lines; `tools/tests/test_voice_lines.py` runs it.

**Timing.** A line takes 0.4 s plus 0.35 s a word of its Russian on the bus, whatever a speech engine's voice does (the
seats' rates are a little fast to stay close to that). A recorded line takes its recording's time instead, so the next
line on the channel doesn't cut it off (27 Oct 2026):

- `CalloutBus::LineTimeS`: a fixed line (the catalogue's words, nothing filled in) with a recording takes its length,
  the speaker's own recording before the shared one, as `PlayRecorded` picks them; on the battery net the radio is
  keyed for the same time (`OnAir`).
- `RecordedLineLengths(dir, crew)` (`crew_voice.hpp`) finds the recordings the game would play and reads their
  lengths from the WAV headers.
- The voice component hands them to the runner (`ScenarioRunner::SetRecordedLengths`) for the language it plays, once
  per runner and language. With no voice installed nothing plays and every line keeps its speaking time.
- Headless runs (the tests, `crew_balance`, `helper_coverage`) play nothing and keep the speaking time.

The listeners learn a line when it ends, so a recording longer than its speaking time makes the crew that much slower
in the game than headless. Above 1.5× time compression the crew isn't voiced, and a recording's time on the bus passes
faster than the voice. Console: `sam3d.Voice`, `sam3d.VoiceLanguage auto|ru|en`, `sam3d.VoiceVolume`.

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
  - the bus's "now";
  - a recorded line takes its recording's time on the bus (the speaker's own, then the shared one; never a line with
    words filled in), and the next line starts when it ends;
  - a WAV's length from its headers (mono, stereo, not 16-bit PCM, no file);
  - the English recordings are found for the seats that say them, and not for the "Air!" lines left to the engine.
- **Unreal automation, headless:**
  - `SAM3D.Crew.Speech`: the engine speaks a contact report, 4.2 s, not silence;
  - `SAM3D.Crew.Bodies`: five bodies in a game world, each head 10 cm from its station's eye point, the camera's seat
    hidden;
  - `SAM3D.Crew.RecordedLines`: with the crew speaking English, the voice component gives the runner the 133 Osa
    recordings' lengths, and every recorded line said in the drill's first minute lasts exactly its recording.

## 7. Not yet

- Skinned meshes and hand-made animations: the rigid parts are placeholders, posed by the same simcore model a
  skeletal mesh would use.
- Lips.
- Players' bodies in multiplayer: another player's seat shows his body at rest.
- Voices on a network client (the client mirrors the console, not the bus).
- Russian voices on this machine, and Russian recordings (Kokoro has no Russian; English first, by the owner's choice).
- Recorded lines for callouts with filled-in words (azimuths, ranges, target numbers): they still go to the engine.
- Stressed recordings: `voices.json`'s stress raises the engine's pitch and rate, not a recording's.
