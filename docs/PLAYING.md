# Playing SAM3D

The player's guide to the first playable test: how to start, the tutorial, the camera views, and every key. The game
itself carries the same guide: the tutorial mission shows it step by step, and **G** shows every key at any time.

## Starting

Build once (or let the editor build on opening the project), then:

```
.\tools\play.ps1 -Tutorial      # straight into the tutorial
.\tools\play.ps1                # the missions menu
.\tools\play.ps1 -Build -Tutorial   # build the code first (close the editor)
```

`play.ps1` runs the game in its own window (`UnrealEditor.exe SAM3D.uproject -game`); the engine is `D:\UE_5.8` unless
`UE_ROOT` says otherwise. The tutorial plays on the Kesmark test range, which needs no Cesium token or internet. Playing
from the editor (Play) works too, but there the editor keeps a few F keys of its own (F8 ejects from the vehicle,
F11 the immersive view): use the standalone game for the full set of views.

## The missions menu

Up/Down a mission, Left/Right where to start (a seat of the crew, the commander's seat, or outside), Enter to play, Q to
quit. The groups:

| Group | What it is |
|---|---|
| **Tutorial** | Start here: the first shoot, with the guide on the screen. You command; the crew waits for your orders. |
| Missions | NPC commanders run the crew and the battery by the drill. Take a seat and do your part, or command. |
| Drills | Scripted shoots: the scenario gives the orders. Watch, or take a seat and keep up. |
| Ranges and history | Range days and the Osa's battles. |
| Air war | Raids planned by the enemy air command (and the campaign screen). |
| Test range | The Kesmark map with a raid at the bridges. |

## The tutorial: your first shoot

You command the Osa 21, cold in its position by the Kesmark bridges, the PU-12M, the reload vehicle and the fuel truck
beside it. Two slow practice drones fly at you, the second ten minutes after the first. Nobody gives the crew orders
but you. The guide (top right) moves on as you do each step; **N** skips a step.

1. **Welcome.** Right mouse + drag turns the camera, the wheel zooms.
2. **The camera views.** Try four of F1-F11 (below). F8 finds the first drone, far to the north-west.
3. **Into the cabin.** F1, then F1 or Tab from seat to seat; come back to the commander's.
4. **Take command.** T: the seat is yours.
5. **К бою! (Action stations!)** O opens your orders; pick it by its number. The crew calls each step on the intercom.
6. **Coming to readiness.** Some minutes from cold: Ctrl+Z speeds time up. Done when the senior operator calls
   *Готов СОЦ!* (search radar ready).
7. **Search.** Wait for his contact report. *Цель … — взять!* (Take target!) is greyed until a contact is reported.
8. **Take the target.** Give it; the crew captures (ЗАХВАТ).
9. **Prepare to launch.** *Приготовиться к пуску!*
10. **Permit the launch.** *Пуск разрешаю!* opens once the drone is in the zone (ЦЕЛЬ В ЗОНЕ).
11. **Missile away.** F6 follows your missile, F3 watches it fly past.
12. **After the shot.** HIT or MISS; the second drone; Esc for the menu and, at the end, the debrief.

A headless player does exactly these steps in `simcore/tests/test_tutorial.cpp` and launches some 7.5 minutes of sim
time in.

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
| Drag or wheel on a hand wheel | Turn it |
| T | Take this seat, or give it back |
| O | Orders (at the commander's seat) or reports (any other); 1-9 pick, 0 next page, Esc close |
| H | Training overlay: guided, hint, demonstrate, off |
| 1-9 | Quick crew drills (outside the crew menu) |
| B | The PU-12 plotting board |
| V | The other vehicle (a battery link) |
| [ ], 1-5, P | PU-12M seats: pick a target, give it to a launcher, plot it |
| G | Every key |
| N | The tutorial's next step |
| Esc | Pause: resume, restart, the missions, quit; the debrief at the end |

## Known limits of this test

- Models are placeholders (docs/scenery/): the Osa is boxes outside, the drones orange shapes. The battery's other
  vehicles are drawn on the ground under their sim position (which is at their antennas).
- With the player in command, *К бою!* doesn't switch the deputy commander's TV on, so the crew data's "ready at a
  halt" task never completes and *К бою!* never greys as done (the NPC commander gets there through the drill
  master). The tutorial waits for the search radar instead. The engagement doesn't need the TV.
- Multiplayer: the views and the guide are each player's own; time keys work on the server only.
- Frame rate: about 100 fps in every view at 1600x900 on the development PC (19 Oct 2026). If the cabin runs slowly,
  `sam3d.CabinDisplayHz 15` and `sam3d.TvHz 10` in the console (~) lower how often the instruments and the TV redraw.
