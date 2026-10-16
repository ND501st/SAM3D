# Multiplayer: one simulation, a crew of players

Written 16 October 2026. Covers the multiplayer crew (players in the vehicle's seats) and the knowledge stream (roadmap
E10). Code: `simcore/scenario_runner.hpp` (seats), `simcore/console_sync.hpp`, `simcore/em/replication.hpp`; Unreal:
`USimCrewSeatComponent`, `USimKnowledgeComponent`, `ASimCrewPlayerController`, `ASimCrewGameMode`, `USimWorldSubsystem`.

Multiplayer serves the crew pillars (`docs/crew/README.md`): players and NPCs share one crew, each seat held by
either, and the NPCs work with the players exactly as with each other (they wait for, remind and report to a player
in a seat; a player commander gets the same reports and acknowledgements as an NPC one).

## The model

* **The server runs the one simulation.** Every machine loads the same scenario (the map's `ASimScenarioDirector` does it
  on all of them), but a network client never steps it: `USimWorldSubsystem::IsMirrorOnly()` is true there and the
  fixed-step loop and the EM layer are skipped.
* **Seats.** A player takes one of the console's stations (`commander`, `deputy_commander`, `senior_operator`,
  `operator`, `driver`) with `ScenarioRunner::TakeSeat`. The NPC there stops acting (and stops aiming its wheel; the
  engagement model's `AimCrew` follows the seats), its work in progress passes to the player, and orders for that seat
  (scenario triggers, the commander's order menu, shouted orders arriving late) become the player's procedure, judged in
  the debrief under the player's name; orders queued while one is under way follow it. Free seats stay NPC-crewed;
  `LeaveSeat` hands the seat back.
* **Hands only at your own station.** `SeatSet` / `SeatTurn` refuse a control whose `station` is another seat's (a
  control without a station is anyone's), with the reason ("'pwr.gta' is at the driver's station").
* **The console is mirrored.** `ConsoleSnapshot` / `ApplyConsoleState` carry every control position and wheel value,
  every subsystem's state and progress and every sim input (218 bytes for the Osa), with a fingerprint of the console's
  ids so two machines with different files refuse each other. Sent unreliably ten times a second; the newest wins. The
  client's cabin, lamps and gauges draw from the mirrored console.
* **What each player knows** comes by its own stream (E10, `docs/EM_ARCHITECTURE.md` §30): the view of the player's
  node, never the truth.

## Unreal

* Use `ASimCrewGameMode` (or put `USimCrewSeatComponent` and `USimKnowledgeComponent` on your own PlayerController).
* `USimCrewSeatComponent::RequestSeat(Role)` (reliable) takes a seat; `SeatRole`, `SeatProcedureId` and
  `SeatProcedureStep` replicate to the owner. Cabin clicks already go through `USimWorldSubsystem::PlayerSetControl` /
  `PlayerTurnWheel`, which route a seated player's moves to the server (reliable for switches, unreliable for wheel
  drags) and return `Sent`; `OnActionResult` reports a refusal with its reason. On the server (or a listen server's own
  player) the move is applied at once.
* Command-line helpers for scripted sessions: `-SimSeat=<station>`, `-SimCrewTry=<control>:<position>`,
  `-SimCrewSmoke=<seconds>`.

## Checks

* simcore: `simcore/tests/test_crew_seats.cpp` (seats replace NPCs and take their orders; station checks; aiming;
  the mirror holds the server's console, refuses old, damaged and foreign snapshots), `test_em_replication.cpp`.
* Unreal, headless: `UnrealEditor-Cmd ... -ExecCmds="Automation RunTests SAM3D.Knowledge;Quit"`.
* Unreal, networked: `tools/ue_crew_smoke.ps1` starts a dedicated server and two clients. Result on 16 October 2026:
  the driver's client started the gas turbine (moved by the server, then mirrored), the commander's client's try at the
  same switch was refused ("'pwr.gta' is at the driver's station"), both clients applied 234 console snapshots in
  30 s, and the server's seats emptied when the clients left. With the EM practice scenario and `-TimeScale 8
  -ClientSeconds 60`: at 466 s of sim time both clients' scopes showed the drone from the stream (one search echo, one
  tracking-radar echo, antenna at 9.9 degrees; the unit node's view held two radar tracks and one picture item), and the
  driver had received the next order (`pto_on_line`) as a player.

* **The scopes come with the knowledge.** Every cabin display draws from one `simcore::ScopeFeed` (`scope_feed.hpp`):
  search echoes, IFF replies, tracking-radar echoes, the elevation indicator's beam and sweep, the antenna elevation, the
  wheels-to-true-azimuth offset, the missiles' ranges and the unit's position. Where the sim runs it comes from the
  engagement model; the server also puts it into each player's knowledge view (`em::ViewScope`), with each echo's
  scenario target id replaced by an opaque key per player (`EchoKeys`), so a client can't tell a drone from chaff by
  its name. On a network client the displays draw from that block, and their sweep runs on the frame clock. The counters
  read from it too (`CounterReading` with echoes and antenna elevation).
* **The practice map plays the EM scenario** (`scenarios/osa_practice_em.json`), so the knowledge views carry the unit's
  radar tracks and picture as well.

* **Board, radio and battery ride along.** Each view also carries `em::ViewExtras`: the PU-12 board's marks of the last
  five plots, the last radio messages, and the battery's other vehicles and dummy positions; a client's HUD and map draw
  from it (roadmap 7.7).

## Not done

* Voice between players (the R-124 intercom is modelled as the order path, not as player audio).
* Which seat a player may take is first come, first served; no lobby or roles screen.
* The 3D world on a client (the target aircraft actors, the TV camera's picture of them) still comes from the client's
  own unstepped world; entity positions need a stream of their own (or Unreal's actor replication).
* NPC crewmate behaviour and teamwork (next).
