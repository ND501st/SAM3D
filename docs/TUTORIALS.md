# The lessons

How SAM3D teaches the Osa and the PU-12M: eleven lessons, one to start and one for each seat of each crew, each a list
of small steps that light up the control to use and wait until it is used. This page is how they are built and checked;
how they are played is `docs/PLAYING.md`; why they are built this way (DCS, flight simulators, Portal, Factorio, the
instructional design research) is `docs/research/TUTORIAL_DESIGN_2026-10.md`.

## The lessons

| Mission (menu: Tutorial) | Lesson file | Seat | Steps | What it teaches |
|---|---|---|---|---|
| Start here: the camera, the cabin, the controls | `data/tutorials/basics.json` | any | 21 | Turning and zooming the camera, the F-key views, the cabin and its seats, one of each kind of control (switch, selector, lock, hand wheel), the keys card, time compression. Free: any control works. |
| Seat lesson 1: the driver | `school_driver.json` | driver | 24 | The ПУ-303, the dashboard and the radio; driving 40 m and halting; «Стою!»; the turbine (ГТА), «Питание подано!», the gyro's arrest; the generator on line; then the crew's engagement from the driver's seat. |
| Seat lesson 2: the operator | `school_operator.json` | operator | 28 | The 0096-9M1 cabinet, the range panel; К бою: equipment, stabilisation, the TV sight's power, unlocking the turret, the drives; «АПУ расстопорено!»; the capture: РАБОТА, СДЦ, the transmitter, the range gate on the echo; «Захват!». |
| Seat lesson 3: the senior operator | `school_senior_operator.json` | senior_operator | 24 | The search radar: ПОИСК, ВЫСОКОЕ under its guard, the beams, the range scale; «Готов СОЦ!»; IFF (ЗАПРОС), the contact report, the azimuth wheel onto the target until the capture. |
| Seat lesson 4: the deputy commander | `school_deputy_commander.json` | deputy_commander | 32 | The orientation (АК—ЗК, the crew chief's Qc on the ЗК-4, РЕЛЕ—Qc, СОГЛАСОВАНИЕ); the elevation wheel and ЦУ; the TV; БР, the launch key, «Цель в зоне!», ПУСК, «Пуск!». |
| Seat lesson 5: the commander, in command | `school_commander.json` | commander | 18 | The crew menu; every order of an engagement in turn, the crew doing each, from «К бою!» to «Пуск разрешаю!». |
| PU-12M lesson 1: the driver-electrician | `school_pu12m_driver.json` | driver | 11 | The ŠČV input board; the AB-1 set out, started and put on line; «Агрегат работает, питание подано!». |
| PU-12M lesson 2: the radiotelephone operator | `school_pu12m_radio_operator.json` | radio_operator | 24 | The R-111 to ДЕЖ. ПР. and the R-123M on, the mast up (standing at its winch), 220 V to the equipment, the VS-27; then the R-111 on the regiment's net (`tune_r111`): the supply on its meter, ТЛФ, 20 %, the preset ЗПЧ 1 and the mechanism turning, НАСТРОЙКА УМ watched on НАСТР. БУМ, the net's level; «Р-111 настроена!», «Связь с КП полка есть!». |
| PU-12M lesson 3: the squad leader | `school_pu12m_squad_leader.json` | squad_leader | 16 | His radio; the GAK run up, the PAB-2 set up, the measured direction entered, the KP-4 and the DTP; «Ориентирование закончено!». |
| PU-12M lesson 4: the operator-plotter | `school_pu12m_operator.json` | operator | 11 | The post deployed: ] picks a target on the IT-45, P writes it on the board; the board kept fresh (the targets plotted again as they close) until the commander gives one to a launcher. |
| PU-12M lesson 5: the battery commander, in command | `school_pu12m_battery_commander.json` | battery_commander | 32 | «Пункт управления развернуть!», the ASPD-U and his radios: the R-407's channels set from scratch (its drums unlocked, set to the regiment's 56.0 and 52.0 MHz, locked again: the lesson's plan is not prepared), ДУПЛЕКС, 4-ПР. ТВ for the ASPD-U, ПУ to РАДИО, the R-111's antenna to the mast; the DT-2 marker onto a target (`marker_azimuth`, `marker_range`), ЦУ, launcher 1; the launcher's report. |

Every seat lesson plays on the Kesmark range (`data/maps/kesmark_range.json`) from a cold halt, a class 1 crew with
NPCs in every other seat and an NPC commander whose orders come at the learner's pace (`on_start: order:`), and one
practice drone held until the lesson calls it (`"on_cue": true`, `release:d1`). The seat lessons end with a real
launch: the learner's last steps are the seat's part in it. The PU-12M's lessons play at the same place: the crew's
from a cold post, the plotter's with the post deployed, the battery commander's with two launchers deployed round it.
The PU-12M's targets fly a racetrack 6 to 40 km north of the post for some 40 minutes, so a learner who takes his time
still finds them on the tube, and in the launchers' reach on the near leg.

The PU-12M's lessons add conditions and aims for the post (`simcore/tutorial.hpp`): `plotted` (the learner wrote a mark
on the board during the step), `marked` (the DT-2 marker is on a target), `designated` (a target went out to a launcher
on the battery net), and the wheel aims `marker_azimuth` and `marker_range` (the nearest mark on the tube). The NPC
battery commander gives targets only from marks under two minutes old, so the plotter's last step asks for a fresh mark
as well as the designation. The commander's own launcher step is `"conditions_only": true`: pressing 1 is asked for, but
the step is done only when the designation has gone out (a marker that slipped off the target is put back and ЦУ pressed
again); his marker steps wait for the battery net, so the mark isn't stale by the time the net is up.

## How a lesson is built

The file format is in `simcore/tutorial.hpp`'s header comment (step actions, waits, start actions, conditions). The
rules every lesson follows, from the research:

1. **One action per step**, gated on the real control: the step is done when the console says the switch is at the
   position (or the wheel on its mark, the order given, the report made), never by a "next" button. Reading steps
   (`"continue": true`) are the only ones the learner moves on himself (Enter).
2. **Show the thing, not a picture of it.** The step names a control of the console (`"control"`, `"wheel"`), or a panel,
   display or lamp of the cabin (`"look"`): the cabin pulses it green and a small lamp lights it (the driver's ПУ-303 is
   otherwise in the dark), a bracket marks it on the screen with the position to set, and an arrow at the screen's edge
   points to it when it is out of view. L turns the head onto it. Words go beside it, never over it (they would hide
   its own label, which the learner must learn to read). The bracket and L aim at the middle of what is drawn of the
   control (a guard's flap stands out in front of its button). A seat's own controls are in reach up to 2.2 m from the
   eye, anything else 1.5 m; L onto a control of the seat beyond arm's reach steps the learner in front of it, 70 cm off
   its panel (the crewman gets up for a far panel, as simcore's `crew_body.hpp` has it): the deputy commander's gyro
   panel, otherwise seen nearly edge-on from his seat.
3. **A tour before the drill** (pre-training): the first steps of a seat lesson show the seat's panels and what they
   are for, before any order.
4. **The real drill.** The steps are the seat's procedure from `data/procedures/osa_ak.json` in its order, and they wait
   for what the procedure waits for (`"procedure"`, its `wait_for`): the lesson can't teach a move before its time.
   The words in «» are the crew's own callouts (`data/crew/osa_ak_crew.json`).
5. **Explain, demonstrate, imitate, practise.** Each step says what (`lines`), why (`why`, one sentence), and the keys
   (`hint`); the seat lessons are followed by the missions, where the same drill runs with no lesson.
6. **Never stall.** A step that waits says what for (`wait_text`); a long wait runs time faster (`"fast_forward"`). A
   headless perfect student plays every lesson to its end in the tests (below), so no step can be left impossible.
7. **Strict where it matters.** A seat lesson (`"strict": true`) lets the learner use only the step's control: anything
   else is refused with the reason ("Not that one: the lesson asks for ..."), the crew hotkeys are off and the seat
   can't be given up. The first lesson is free.
   In every lesson Backspace goes back a step (shown again to read, its orders not given again; what was done stays
   done) and N skips one: a step with nothing to do is passed, otherwise the instructor does it, as soon as its wait
   allows (a held wheel follows its mark), with the ghost hand showing the move. A student who only presses N is
   taken through every lesson to its launch in the tests.
8. **Immediate feedback.** A step done flashes a tick and its title at the top of the screen; an order or a report
   counts when the learner gives it, though a busy intercom says it later (`OrdersGiven`, `ReportsMade`); a wrong move
   on the step's control says what happened (`Feedback`); a multi-position selector turned one click toward its target
   position is a move on the way, not a mistake (`StepResult::Toward`).
9. **Hints that grow.** The keys are on the card from the start; after 12 s on an action the hint pulses; after 25 s it
   adds "Can't find it? Press L" and a ghost hand shows the move (not before: it would cover the control and its
   label).
10. **An instructor's voice.** Each new step's title (and an action step's first line) is read aloud in an English voice
    (`USimCrewVoiceComponent`; the Russian in Latin letters, `simcore::InstructorSpeech`; `sam3d.InstructorVoice 0` to
    silence it). The crew's own voices stay on the intercom.

## The step card

Top right (the left while the plotting board is up), `ASimCabinHUD::DrawGuide`:

- The lesson and chapter, the step's number of the count, and a progress bar.
- The title, and for an action the control and its position ("ДАЛЬНОСТЬ  >  0–35 km") or the order or report in its
  Russian and English words; for a wheel, a gauge: the wheel now and the mark (azimuths in thousandths, ranges in km),
  the tolerance as a green band, and which way to turn.
- The lines, then "Why: ...", the hint, and a status line that always says what happens next: "Enter: continue",
  "Your move.", "Waiting for ...", "Watch: waiting for ...", or, away from the lesson's seat, "Back to your seat first".
- The crew menu (O) opens below the card, on the page with the step's line, marked "<-- this one".
- The card's sizes follow the window (laid out for 900 px high, up to twice that); UE5's small, medium and large
  engine fonts are one size, so the title and the action are scaled text.

## Adding a lesson

1. A scenario in `data/scenarios/` (copy a `school_*.json`: a cold halt, the crew, an `"on_cue"` target).
2. A lesson in `data/tutorials/` naming that scenario, and its seat if it is a seat lesson.
3. An entry in the Tutorial group of `data/scenarios/missions.json` with `"tutorial"` and `"start"` (the seat, or
   `outside`).
4. `simcore/tests/test_tutorial.cpp` and `test_missions.cpp` count the lessons: add it there; the perfect student then
   plays it through.

## Checking

- `simcore_tests "tutorial"`: every lesson loads and binds (every control, position, order, report, panel and lamp it
  names exists), the strict refusals, and a perfect student plays each lesson to its end (the seat lessons to a
  launch), printing the step and the learner's moves where one gets stuck.
- Unreal: `SAM3D.Missions.Menu` (the catalogue), `SAM3D.Missions.Tutorial` (the first lesson), `SAM3D.Missions.SeatLesson`
  (a strict lesson: the tour, the refusals, the marker's place in the cabin, a switch moving it on).
- In the game: `-SimTutorialAutoplay=<folder>` plays the lesson as a learner would (the head turned onto each step's
  control, a click a position, the wheels turned at a hand's pace, the menu opened for orders and reports) and
  photographs each step's card (`NN_<step>.png`), then quits 4 s after the last step. It logs a warning when, after L,
  the cross isn't on the step's control (out of reach, or another control in front): a person couldn't click it.

```
UnrealEditor.exe SAM3D.uproject "/Game/Maps/KesmarkRange?SimMission=school_operator?SimSeat=operator?SimTutorial=tutorials/school_operator.json" -game -windowed -ResX=1600 -ResY=900 -SimTutorialAutoplay=out/lessons/operator
```

## Known limits

- The take-target order's words have the target's number in them; without a designation from the PU-12 the number is
  shown as "…" (on the card and in the crew menu) and the order, which the crew still carries out, goes unspoken.
- With the player in command, «К бою!» leaves the TV off (`docs/PLAYING.md`, known limits); the commander's lesson orders
  the TV look separately.
- English only: the cards and the instructor; the crew's words are in Russian with their English beside them.
- Seen from the deputy commander's seat, the ЗК-4 knob at the gyro panel's far end is hidden behind a part of the cabin
  model; L steps him in front of the panel, where it shows (crewmates' bodies within 60 cm of his eyes are hidden while
  he stands there).
