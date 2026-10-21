# Playing SAM3D

The player's guide to the first playable test: how to start, the lessons, the camera views, and every key. The game
itself carries the same guide: the lessons teach it step by step, and **G** shows every key at any time.

## Starting

Build once (or let the editor build on opening the project), then:

```
.\tools\play.ps1 -Tutorial                  # straight into the first lesson
.\tools\play.ps1 -Lesson senior_operator    # a seat's lesson (driver, operator, senior_operator, deputy_commander, commander)
.\tools\play.ps1                            # the missions menu
.\tools\play.ps1 -Build -Tutorial           # build the code first (close the editor)
```

`play.ps1` runs the game in its own window (`UnrealEditor.exe SAM3D.uproject -game`); the engine is `D:\UE_5.8` unless
`UE_ROOT` says otherwise. The lessons play on the Kesmark test range, which needs no Cesium token or internet. Playing
from the editor (Play) works too, but there the editor keeps a few F keys of its own (F8 ejects from the vehicle,
F11 the immersive view): use the standalone game for the full set of views.

## The missions menu

Up/Down a mission, Left/Right where to start (a seat of the crew, the commander's seat, or outside), H the helper (Off,
Hints, On: below), Enter to play, Q to quit. The groups:

| Group | What it is |
|---|---|
| **Tutorial** | Start here (the camera, the cabin, the controls), then a lesson for each seat of the Osa. |
| Missions | NPC commanders run the crew and the battery by the drill. Take a seat and do your part, or command. The HARM drill and the Shrike drill are the ARM threat: in the senior operator's seat watch the scope for a small fast mark coming straight at you, or a target that came at you turning away, and call it; in the commander's, everything off once your missile in the air has burst. |
| Drills | Scripted shoots: the scenario gives the orders. Watch, or take a seat and keep up. |
| Ranges and history | Range days and the Osa's battles. |
| Air war | Raids planned by the enemy air command (and the campaign screen). |
| Test range | The Kesmark map with a raid at the bridges. |

## The helper

Turn it on in the missions menu (H; kept for every mission) or in a mission (H, also in the pause menu). It shows the move
the crewman in your seat would make now, the way a lesson shows its step but without the lesson's card: the control
pulses green, the crew menu's line is green for an order or report, at the post the target's number is starred, and a
line at the top says what and why. *On* shows it at once, *Hints* once you hesitate longer than your seat's crewman
would, *Watch the crewman* (H in the cabin) what the NPC at a seat you haven't taken does next. N lets it make the move
for you. In command it is the NPC commander thinking in your place: his next order. `docs/crew/14_HELPER.md`.

## The lessons

Eleven lessons, first in the missions menu (`docs/TUTORIALS.md` lists what each teaches). Start with **Start here**: the
camera, the views, the cabin, and one of each kind of control. Then the seats, in any order: **the driver**, **the
operator**, **the senior operator**, **the deputy commander**, and **the commander, in command**. Each seat lesson puts
you in that seat of the Osa 21, cold at a halt by the Kesmark bridges, with the rest of the crew at their posts. It
shows you your panels first, then takes you through your part of the drill one switch at a time, from «К бою!»
(action stations) to the launch at a practice drone. The commander's orders come when you are ready for them. In
the commander's own lesson the battery's PU-12M is on the battery net: you turn your A-1 between the intercom (ВС) and
the net (Р-123), report the vehicle ready, and read back the target the battery commander gives you.
Then the **PU-12M**, the battery's command post, seat by seat: the driver-electrician (the AB-1 generator), the
radiotelephone operator (the radios, the mast, the telecode's path, the air picture read out to the plotter, the sets
kept on frequency when a net moves or is jammed), the squad leader (orienting the post, the radio data, the check of
communications),
the operator-plotter (the radiotelephonist's reports onto the board, each new target called aloud to the commander) and the battery commander (deploying the post, his
RTK-1, the regiment's ПТ-1 group read by hand against the talk table on his desk, a target to a launcher with the DT-2
marker and the ДХ keys and its read-back, the battery net out of jamming, a telephone line answered).

**Headsets, boxes and lines.** Every crewman wears a шлемофон plugged into his box (docs/crew/12_COMMUNICATIONS.md). With
it off (J) you hear only voices in the cabin, and not every one over the turbine: no intercom, no radio, no telephone.
The box at your seat decides what your headset is on: the intercom, or a radio (K and Shift+K turn it; on a radio you
still hear the intercom). The Osa's commander has the A-1 (intercom or one of the two R-123M), the deputy the A-2, the
driver the ПВ; the senior operator and the operator are on the intercom. In the PU-12M the commander's RTK-1 talks on one
set and listens to a second, and has five telephone lines; the PVS-1 (radiotelephonist), PVS-2 (squad leader) and PVS-3
(driver) each pick the intercom or a set; the plotter has no box. A ringing line or the Osa's TA-57: Y answers, Y again
puts it down. The bottom left of the screen shows your headset, your box and the sets it reaches (● alive), a call and
a ringing line; the radio log above it shows only what your seat heard.

**The papers.** On the radiotelephonist's table lie his radio data (each net's frequencies by number, the call signs, the
changes as Ч+minutes, the telecode channel, the lines); on the commander's desk the talk tables (ПТ-1, and the signal table
ПТ-2 with its flares). They are paper, in Russian as issued, and as light or dark as the cabin round them.

**On the march** the battery keeps radio silence but for fire control (the 1991 textbook): the battery commander's march
orders reach the column as flag signals (lamps at night), and the post's orders on its nets are refused until the battery
has to fight. A raid within reach opens the radio again.

In the PU-12M the plotting board folds on its hinge; the operator-plotter swings it open by its latch. He writes on it
what the regiment reads out on the air-picture net, square by square, out to 200 km (the IT-45 is the battery
commander's: the plotter doesn't copy it): with the cross on the board the mouse wheel picks the number of a report
heard (a small chip beside the cross), a click writes it there. Within
5 cm of the right square the mark is exact; further off it lands off the target. The air-picture lines fade in a few
seconds: remember the square. He calls each new target to the commander aloud as he writes it (O, then «Новая цель …»,
filled in from the board), and a turn or a lost target the same way: the NPC commander has a new target from that call,
or only half a minute later from his own look at the board, and a report left unsaid for two minutes is missed in the
debrief. The battery commander
reads it for warning: his aids line gives each track's range and when it will be here, and the regiment's last
two-digit command. On the ДХ, СВ / Ч / ГР / Н1-Н3 set friend, foe, group and height zone on the target in the window; the
ФР block's КОНТРОЛЬ position puts a training echo on the tube; the switch on the ФР block picks the cabled radar's type.
Radio traffic is spoken through a radio voice and drawn in its net's colour (amber the air picture and the
radiotelephonist's read-out, cyan the command net, blue the battery net, violet the regiment's net, green the covered
regiment's, pale yellow a telephone line). docs/research/PLOTTING_BOARD_2026-10.md.

How a step works:

- **The card** (top right) says where you are (the lesson, the chapter, step n of N), what to do, the control and the
  position to set it to (or the order or report to give), why, and the keys. Its last line always says what happens
  next: *Enter: continue* to read on, *Your move*, or *Waiting for ...* when the drill isn't ready for the move yet.
- **The control is lit**: it pulses green in the cabin with a small lamp on it, a green bracket on the screen marks it
  with the position to set, and an arrow at the edge of the screen points to it when it is out of view. **L** turns
  your head onto it.
- **Only the lit control works** in a seat lesson; anything else says why not. Do the step and the next one comes up,
  with a tick at the top of the screen. A hand wheel shows a gauge: where the wheel is, the mark, which way to turn.
- **Orders and reports**: O opens the crew menu on the page with the step's line, marked *<-- this one*; pick it by its
  number. It counts at once; the intercom says it when it is free.
- **Waits** run time faster on their own (the turbine spinning up, the radar warming); it goes back to normal for your
  next move.
- **The instructor** reads each new step aloud (English; `sam3d.InstructorVoice 0` in the console silences him).
- **Backspace** goes back a step to read it again (what was done stays done; Enter goes on). **N** skips a step: the
  instructor does it for you (his ghost hand shows the move), so the drill goes on as if you had.

A headless perfect student plays every lesson through in `simcore/tests/test_tutorial.cpp`; each seat lesson ends with
a launch some 7.5 minutes of sim time in.

## Camera views

As in DCS: each F key is a kind of view, and pressing it again moves to the next of its kind.

| Key | View | Again |
|---|---|---|
| F1 | The cabin, at a crew seat | the next seat |
| F2 | Round your own vehicle | closer / further |
| F3 | Fly-by: the camera waits ahead of what it follows and watches it pass | a new place |
| F4 | Chase, behind what the camera follows | from ahead |
| F5 | The nearest aircraft, seen from your vehicle, the lens closed on it | the next nearest |
| F6 | Your missile in flight (the newest first); it stays where the missile ended | the next missile |
| F7 | Ground units: your vehicle, then the battery's others | the next |
| F8 | Aircraft | the next |
| F10 | The map, from above | back to the middle, the first height |
| F11 | Free camera: W A S D, Q down, E up, Shift faster, wheel the speed | |

Right mouse + drag turns the camera (F2, F7, F8; looks in F11; pans in F10). The wheel brings it closer or further,
closes F5's lens, sets F10's height. F3, F4 and F10 follow whatever the last view followed: F8 then F4 chases the
drone. F5 looks through the air as it is: a drone 50 km out is lost in the haze, and comes out of it as it closes.

The views are the game's cameras, not the crew's knowledge: they never tell the crew anything, and their labels carry
no ranges or heights (docs/command/: the player's character knows only what reaches it). The console variable
`sam3d.Views` limits them for a stricter game: 1 our own side only (the cabin, our vehicles, our missiles, the map),
0 the cabin and our own vehicle.

## Time

Ctrl+Z faster (x2 up to x32), Alt+Z slower (to x0.25), Shift+Z normal. The top of the screen shows TIME xN while it
is not 1. The simulation keeps its fixed step; only how many steps a frame runs changes. Single player and the server.

## Every key

| Keys | What |
|---|---|
| F1-F8, F10, F11 | Camera views (above) |
| F9 | The trainer's instructor panel (roadmap 10.3; where the simulation runs: single player or the server) |
| Right mouse, wheel | Turn the camera, closer / further |
| Ctrl+Z, Alt+Z, Shift+Z | Time faster, slower, normal |
| W / S, A / D, Space | Drive, steer, brake (a halt for the crew): from the driver's seat only in a mission (from the others the commander orders it) |
| C | In and out of the cabin |
| Tab | The next seat |
| Mouse (cabin) | Look round; the cross names the control |
| Left / right click | Switch on, step forward, press / step back |
| Drag or wheel on a hand wheel | Turn it; held, your eyes go to the display it is worked against (the scope, the range indicator, the IT-45) and come back when you let go |
| X | Get up and walk in the cabin (W A S D, the mouse looks round, any control in arm's reach works); X by a seat sits you down in it, T then takes it over |
| F | Out of the vehicle and back (your seat taken, T): under a hatch, or in the seat below it, the hatch opens and you climb out onto the roof; at the roof's edge by a wheel, down; on the ground by a wheel, up; on the roof by a hatch, in. Each climb takes its real time, seen from your eyes |
| W A S D, Shift (outside) | Walk on the roof (not past its edges or onto the antenna unit or the mast) or over the ground at the drill pace; Shift runs. Never through the hull |
| Click a job's case inside | A job done outside (the PU-12M's AB-1 set and PAB-2 aiming circle, the Osa's aiming circle): you are walked out to it and do it there. Out there, look at it within 3 m and click |
| T | Take this seat, or give it back |
| O | Orders (at the commander's seat; the Osa commander's own words on the battery net follow them: his readiness, a read-back) or reports (any other); 1-9 pick, 0 next page, Esc close |
| H | The helper: off, hints, on, watch the crewman (kept for the next missions) |
| N | The helper makes its move for you (outside a lesson) |
| J | Your headset on or off |
| K / Shift+K | Turn your intercom box (ВС, РСТ-1..4; the Osa's А-1: off, intercom, radio) |
| Y | Answer a ringing line or telephone; again: put it down |
| 1-9 | Quick crew drills (outside the crew menu) |
| B | In the PU-12M: its plotting board. In a launcher: the designations the crew has heard (the board is in the post) |
| V | The other vehicle (a battery link) |
| [ ], 1-5 | PU-12M battery commander: pick a board target, give it to a launcher |
| Wheel, click on the board | PU-12M plotter: pick the number to write, write it where you click |
| G | Every key |
| Enter | A lesson's reading step: continue |
| L | A lesson: look at what the step lights up (from the lesson's seat) |
| N | A lesson: skip a step (the instructor does it) |
| Backspace | A lesson: back a step |
| Esc | Pause: resume, restart, the missions, quit; the debrief at the end |

## Known limits of this test

- Models are placeholders (docs/scenery/): the Osa is boxes outside, the drones orange shapes. The battery's other
  vehicles are drawn on the ground under their sim position (which is at their antennas).
- With the player in command, *К бою!* doesn't switch the deputy commander's TV on, so the crew data's "ready at a
  halt" task never completes and *К бою!* never greys as done (the NPC commander gets there through the drill
  master). The commander's lesson waits for the search radar instead and orders the TV look on its own. The
  engagement doesn't need the TV.
- The lessons are in English; the crew's words are Russian with the English beside them.
- Multiplayer: the views and the lessons are each player's own; time keys work on the server only.
- Frame rate: about 100 fps in every view at 1600x900 on the development PC (19 Oct 2026). If the cabin runs slowly,
  `sam3d.CabinDisplayHz 15` and `sam3d.TvHz 10` in the console (~) lower how often the instruments and the TV redraw.
