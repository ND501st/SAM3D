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

Up/Down a mission, Left/Right where to start (a seat of the crew, the commander's seat, or outside), Enter to play, Q to
quit. The groups:

| Group | What it is |
|---|---|
| **Tutorial** | Start here (the camera, the cabin, the controls), then a lesson for each seat of the Osa. |
| Missions | NPC commanders run the crew and the battery by the drill. Take a seat and do your part, or command. |
| Drills | Scripted shoots: the scenario gives the orders. Watch, or take a seat and keep up. |
| Ranges and history | Range days and the Osa's battles. |
| Air war | Raids planned by the enemy air command (and the campaign screen). |
| Test range | The Kesmark map with a raid at the bridges. |

## The lessons

Eleven lessons, first in the missions menu (`docs/TUTORIALS.md` lists what each teaches). Start with **Start here**: the
camera, the views, the cabin, and one of each kind of control. Then the seats, in any order: **the driver**, **the
operator**, **the senior operator**, **the deputy commander**, and **the commander, in command**. Each seat lesson puts
you in that seat of the Osa 21, cold at a halt by the Kesmark bridges, with the rest of the crew at their posts. It
shows you your panels first, then takes you through your part of the drill one switch at a time, from «К бою!»
(action stations) to the launch at a practice drone. The commander's orders come when you are ready for them.
Then the **PU-12M**, the battery's command post, seat by seat: the driver-electrician (the AB-1 generator), the
radiotelephone operator (the radios and the mast), the squad leader (orienting the post), the operator-plotter (the
regiment's picture onto the board) and the battery commander (deploying the post, then a target to a launcher with the
DT-2 marker and the ДХ keys).

In the PU-12M the plotting board folds on its hinge; the operator-plotter swings it open by its latch. He writes on it
what the regiment reads out on the air-picture net, square by square, out to 200 km, and what the IT-45 shows nearer: with
the cross on the board the mouse wheel picks the number (a small chip beside the cross), a click writes it there. Within
5 cm of the right square the mark is exact; further off it lands off the target. The air-picture lines fade in a few
seconds: remember the square. The battery commander
reads it for warning: his aids line gives each track's range and when it will be here, and the regiment's last
two-digit command. On the ДХ, СВ / Ч / ГР / Н1-Н3 set friend, foe, group and height zone on the target in the window; the
ФР block's КОНТРОЛЬ position puts a training echo on the tube; the switch on the ФР block picks the cabled radar's type.
Radio traffic is spoken through a radio voice and drawn in its net's colour (amber the air picture, cyan the command
net, blue the battery net). docs/research/PLOTTING_BOARD_2026-10.md.

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
| Right mouse, wheel | Turn the camera, closer / further |
| Ctrl+Z, Alt+Z, Shift+Z | Time faster, slower, normal |
| W / S, A / D, Space | Drive, steer, brake (a halt for the crew) |
| C | In and out of the cabin |
| Tab | The next seat |
| Mouse (cabin) | Look round; the cross names the control |
| Left / right click | Switch on, step forward, press / step back |
| Drag or wheel on a hand wheel | Turn it; held, your eyes go to the display it is worked against (the scope, the range indicator, the IT-45) and come back when you let go |
| X | Get up and walk in the cabin (W A S D, the mouse looks round, any control in arm's reach works); X by a seat sits you down in it, T then takes it over |
| T | Take this seat, or give it back |
| O | Orders (at the commander's seat) or reports (any other); 1-9 pick, 0 next page, Esc close |
| H | Training overlay: guided, hint, demonstrate, off |
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
