# 14. The helper: what your seat's crewman would do now

Asked for by the project owner on 20 October 2026: an assistant that can be turned on in the menu and shows the right
buttons and switches to press, like a lesson without its instructions, in every scenario and standard mission, based on
how an NPC in the player's seat would work, and easy to keep up to date as the game changes. Built the same day
(`simcore/seat_duty.hpp`, `simcore/helper.hpp`, `ScenarioRunner::NpcDoes`, the Unreal front end).

## 1. The idea

The helper does not have rules of its own. **It asks the NPC who would sit in the player's seat.** The crew's behaviour
lives in `ScenarioRunner`, each act of an NPC behind a check that an NPC sits at the seat. When a player holds the seat
that act used to vanish. Now it goes through one call, `NpcDoes(seat, SeatDuty)`: an NPC there does it as before; a
player there **owes** it, and the duty is kept until he does it, it stops being due, or the moment passes. The helper
shows the player his seat's duties, in the same terms a lesson uses, so the front end highlights them the same way.

So whatever the NPC crew does, the helper can tell the player who sits there to do it, and when the crew's behaviour
changes the helper changes with it. Pillar 6 (the perfect performance) and pillar 3 (every action has an owner seat) are
what make this possible: every act already belongs to a seat.

## 2. Where a move comes from, best first

| Source | What | Where it is worked out |
|---|---|---|
| duty | What the seat's NPC would already have done: a report (a lamp came on, a new echo, no IFF reply, the hit), an own-initiative drill (ЗАПРОС on a new echo, retuning a set), the commander's single-switch orders, a target to give a launcher, a report to write on the board, a ringing line, the halt | the NPC rules themselves, through `NpcDoes` |
| drill | The next step of the drill ordered to the seat (the commander's order lands in the seat's tracker): exactly what its NPC would do next. A step whose switch is already set is passed by itself, as an NPC's is | the seat's `ProcedureTracker` |
| wheel | A hand wheel the seat's NPC would be turning onto the target now: the tracking radar's azimuth, elevation and range wheels, the course setter (Qc) | the engagement model's own aim (`EngagementModel::WheelWanted`) |
| wait | The ordered drill's next step, waiting for a lamp or another seat: what it waits for | the step's `wait_for` |
| ahead | Nobody will order the seat (no NPC commander: a scripted drill, free play): the drill master's best move toward the crew's task | `DrillMaster::NextFor`, memoised on the console's state |

**In command**, the player's seat is the commander's, and the NPC commander's whole think (`UpdateNpcCommander`) runs as
the **shadow commander**: the same decisions, made in the player's place, with every act stopped at its root
(`GiveCrewOrder`, `GiveOrder`, `NpcOrder`, `SpeakOrder`, `Say`, `Event`, and the few direct acts in the think) and turned
into the player's duty. An order he would give is an order duty; the drills he would hand the seats become the menu
order that starts most of them (К бою, not each seat's drill). An order the player gave in the last minute counts as
the shadow's own, so the shadow moves on to the next decision as the NPC would. The shadow is the ideal crew: no skill
slips, no chance drawn. It thinks only where the scenario has an NPC commander (`"crew_commander": "npc"`): in a
scripted drill the scenario gives the orders.

The marks for the wheels are the engagement's own aim, recorded where the NPC would bring each wheel (also for a wheel
in a player's hands). A mark on the target the commander ordered taken is held while that target is missing from a few
sweeps of the search radar (a low flier in the clutter), so it doesn't flip to the next echo.

The tracking wheels are advised in words, not as a figure to chase: the azimuth wheel brings the tracking radar's azimuth
line onto the target's mark on the search scope, the elevation wheel the target into the TV's crosshair (or onto its mark
on the elevation indicator), the range wheel the range gate onto the echo, each kept there. A wheel counts as on the mark
within ±0.5° (the narrow TV's field) or 500 m; the course setter keeps its figure, the Qc the crew chief gave. A wheel the
equipment turns itself is never advised and won't turn by hand (`ScenarioRunner::WheelDriven`, `SeatTurn` refuses with
"Hands off: ..."): the angles and range while the tracking radar holds the target, the elevation while the elevation
search scans, the range under ПОЛУАВТОМАТ. The reason stays on the wheel while the player points at it. A wheel that
won't turn says why the same way: its lock ("Locked: ..."), or its stop (the tracking antenna's ±330°: "At its stop ...").
A target past a wheel's stop (beyond the range wheel's 28 km) is advised only as far as the stop.
The helper's ghost hand shows switches only, never a wheel.

At the **PU-12M** the battery commander's fire distribution runs for a player in his seat too: each target it would give
a launcher is a designate duty. The plotter owes every report he has heard and not written; the radio operator owes the
air reports he heard and hasn't read out, a retune when his net is ordered to its other frequency, and the jamming he
recognised.

A report the player then makes from the crew menu says what the NPC would have said, and tells the crew what his would
have: the duty carries the words and the knowledge (the target, its IFF answer), so the commander learns the target is
interrogated from the player's «Ответа нет!» as from his NPC's.

## 3. In the game

- **The missions menu**: the *Helper* line, H or a click: Off, Hints, On. Kept in `GameUserSettings.ini` (section
  `SAM3D.Helper`), applied to every scenario that starts: missions, lessons' scenarios, campaign raids, command-line
  scenarios, both vehicles of a battery link. `?SimHelper=<level>` overrides it for one start. An exam keeps it off.
- **The pause menu (Esc)**: H turns it the same way.
- **In the cabin**: H off, hints, on, watch the crewman (the NPC at an untaken seat: what he moves next); N lets the
  helper make its move for you (outside a lesson, where N is the lesson's skip).
- **What it shows**, like a lesson's step without its card: the control pulses green in the cabin (a waiting one too,
  so it is found ahead); the crew menu's line is green for an order or a report; at the post the target's number is
  starred on the board's list and the launcher on the IT list; one line says what (and the next two moves dimmer), and
  one dim line why. The ghost hand reaches for the control once the player has waited longer than his seat's NPC would
  (his patience at his tier). *Hints* shows nothing until then.
- With a lesson running the lesson's highlight wins and the helper is quiet.

| Level | `AssistLevel` | Shows |
|---|---|---|
| Off | unassisted | Nothing (the NPCs still remind you as a crewmate) |
| Hints | hint | The move once you hesitate longer than your seat's NPC would |
| On | guided | The move at once |
| Watch the crewman | demonstrate | At a seat you haven't taken: what its NPC moves next |
| (an exam) | exam | Nothing |

## 4. Keeping it up to date (for whoever changes the crew next)

1. **New NPC behaviour at a seat goes through `NpcDoes(seat, SeatDuty::...)`, never through a bare `NpcAt(seat)` check
   followed by the act.** `NpcAt` is only for choosing who speaks and how fast. Then the helper shows the new behaviour
   to a player in that seat without any other change. The test *"every NPC act at a seat goes through NpcDoes"* counts
   the `NpcAt(` uses in `scenario_runner.hpp` and fails when the count changes, to make you look.
2. **New acts inside the NPC commander's think** must not touch the world when `shadow_` is set: through the root acts
   (above) they don't; a direct act (setting a control, an input, a battery order, a debrief line) needs `if (!shadow_)`.
   The test *"the shadow commander and the wheels' advice change nothing in the world"* compares a scenario with and
   without the helper, and fails if anything leaks.
3. **A new kind of act** with no `SeatDuty::Kind` yet: add the kind (`seat_duty.hpp`), its done check
   (`ScenarioRunner::DutyDone`), its move (`detail::HelpFromDuty`, `HelpDo` in `helper.hpp`), and its presentation in
   `ASimCabinHUD::DrawCrew`.
4. **Run the coverage map** after changing the crew, the consoles or the missions:

   ```
   ./simcore/build/helper_coverage data docs/crew/generated/helper_coverage.md
   ```

   Every mission of the missions menu is played from every seat by a player who only does what the helper shows
   (`HelpFollower`: one move per reaction time, a hand on a wheel turning at the NPCs' rate), against the same mission
   played by the NPC crew. Where the follower does worse (a task never reached or a minute later, fewer kills, no
   launch, fewer designations, a fault) the helper misses something that seat's NPC does: the *Gaps* list.
   `helper_coverage data - 900 <mission>:<seat> --trace --debrief` shows, as it happens, every move, every refusal and
   why, every duty owed, and the debrief: the way to find what is missing. The report also lists every NPC rule the
   helper passed on, and the drills the NPC commander hands out that no order of the crew data starts.
5. `test_helper.cpp` plays the crew practice from every seat by following the helper and checks the crew is ready,
   launches and hits as the NPC crew does; keep it passing.

## 5. Not yet (21 October 2026)

From the first coverage map (`generated/helper_coverage.md`):

- **The TV camera in К бою, in command.** The NPC commander switches the deputy's TV camera on (РАБ.РЕЖ., `tov.power`)
  with an order of its own for that one switch; no order of the crew data starts it (it is in the TV-watch, low-target
  and EMCON drills, each a different order). So a player in command, with or without the helper, never gets the vehicle
  "ready" by the crew data's definition, though the engagement goes on by radar. Needs a sourced order or a decision
  that the deputy switches the TV on in his part of К бою (data, the project owner's call); the coverage map lists it.
  The same holds for the PU-12M radio operator's R-111 power mode (МОЩНОСТЬ 1 %), which the NPC commander orders as a
  single switch.
- **Acts with no move in a player's menu yet**: the Osa commander turning his set to the battery net's other frequency
  (`HearBatteryNetOrder`, `UpdateOsaNet`), asking for the fuel truck and the transport-loader, passing the PU-12's
  designations to the crew, his reports on the battery net (hit, miss), and the battery's short halt on the march when
  the battery drives the vehicle (`osa_mission_march` in command: no halt, so no engagement). The NPC does them; a player
  can't, so the helper doesn't ask. They are among the `NpcAt` uses the tripwire test counts.
- **In command, missions that hang on the TV**: against decoys the NPC commander launches only at what the TV has shown
  him; with the TV camera off (above) the shadow commander waits as the NPC would, so no launch (`osa_decoys`,
  `air_war_nato_decoys`, the battery link's practice).
- **Crew seats where the follower still does worse** (the *Gaps* list): mostly the senior operator capturing later
  than his NPC where several echoes are on the scope (the ranges, the air war's strikes), the deputy's TV search in the
  EMCON engagement (`osa_emcon_tv`), a capture a minute later against decoys, fewer kills at night in Angola. These are
  timing and target-choice leads (the duties are there; the follower turns the wheels onto the mark the NPC's aim gives,
  and where that mark moves between echoes a capture can come later), for the next pass with `--trace`.
- **Multiplayer**: the duties are worked out where the scenario runs (the server); a client mirrors the console but not
  the duties, so a client's helper is quiet.
