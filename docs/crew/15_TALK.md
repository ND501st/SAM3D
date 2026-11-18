# 15 — The crew's talk: turn-taking, the ARM moment, stress, the menu, understanding (roadmap 8.15)

Written 27 October 2026, from the communications brief of 26 October 2026 (the order of work: turn-taking, the ARM
chain, the menu, graded stress, voices, understanding, NPC to NPC). It builds on `04_COMMANDS_AND_CALLOUTS.md` (what is
said), `10_BODIES_AND_VOICES.md` (how it sounds) and `12_COMMUNICATIONS.md` (who hears it). The project owner's rule for
all of it: **no made-up instructions or commands.** Nothing here adds a phrase to the catalogue; it changes when and how
the catalogue's lines are said, and how the player finds and says them. Where a rule is ours it is marked *sam3d*.

## 1. Where it lives

| Part | Code | Data | Tests |
|---|---|---|---|
| Turn-taking | `simcore/crew_callouts.hpp` `CalloutBus` | – | `test_crew_talk.cpp`, `test_crew_voice.cpp` |
| The ARM moment | `ScenarioRunner::ThinkAsCommander` (the ARM block), `ArmEmission` | `osa_ak_crew.json` task `silent` | `test_crew_talk.cpp`, `test_arm_drill.cpp` |
| Graded stress | `ScenarioRunner::UpdateStress`, `CrewMember::SetStressLevel` | scenario `"setting"`, `skill_levels.json` `stress_factor` | `test_crew_talk.cpp` |
| The voice | `simcore/crew_voice.hpp` `VoiceFor`; Unreal `USimCrewVoiceComponent` | `voices.json` `stress` | `test_crew_talk.cpp` |
| The menu | `crew_data.hpp` `MenuLines`, `PlaceInMenu`, `OrderDoer`; Unreal `ASimOsaVehicle` (Caps Lock), `ASimCabinHUD::DrawCrewMenu` | `<system>_crew.json` `"menu"` | `test_crew_talk.cpp` |
| Replies | `ScenarioRunner::PlayerReply`; Unreal U / I / P | – | `test_crew_talk.cpp` |
| Glosses | `crew_voice.hpp` `GlossLine`; the HUD's subtitles | `voices.json` `"gloss"` | `test_crew_talk.cpp` |
| What I've heard | `ScenarioRunner::PictureOf`; `USimWorldSubsystem::GetSeatPicture`; the B page | – | – |

## 2. Turn-taking on the bus

One voice at a time per channel (the intercom, the battery net, the cabin) was already the bus's rule (8.4). Added:

| Rule | What it does | Basis |
|---|---|---|
| A moment between speakers | The next line on a channel starts 0.2 s after the last one ended (`kTurnGapS`). | Conversation's modal gap between turns is about 200 ms across ten languages (`stivers_2009`). |
| Urgent lines break in | A priority-0 line due on a channel where a less urgent line is being said cuts it off; the cut line is said again after, whole (unless it goes stale), and is kept apart from the log (`CutOff()`): nobody heard it whole. An urgent line never cuts another urgent one, and doesn't wait for the gap. | *sam3d*: the drill's warnings ("Срыв сопровождения!", "Пуск ПРР!") cannot wait behind a routine report. |
| Stale lines are dropped | A line left waiting past its shelf life is not said (`ShelfS`): «Есть!» / «Выполнено!» after 5 s, a target's position after 8 s (a fresh report follows), a reminder after 10 s, any other report after 15 s. Orders, warnings, urgent lines and radio traffic never go stale. A player's own line is never dropped. | *sam3d*: an acknowledgement 20 s late tells the commander nothing; a position 10 s old is wrong. |
| One caller per fact | A report or warning in the same words from a second seat, while the first is waiting, being said or said less than 8 s ago, is not said again (`Merged()`). | *sam3d*; the drill gives each report to one seat (`04` §3.6). |
| An NPC doesn't talk over what is in his ear | An NPC doesn't start a line that isn't urgent while another channel he listens to is busy (the runner's `Listens`: the cabin out of doors, the intercom through his headset and box, the net through its set). The player may. | *sam3d*: a crewman waits for the net to finish before he speaks on the intercom. |
| Urgent warnings with the call | Priority-0 lines and warnings are made with ВЫЗ / ЦИРКУЛЯР: heard by boxes on the radio too. | Built with 8.13 (`r124_bmp2`: ВЫЗ pulls subscribers on the radio back to the intercom). |
| Recorded lines take their time | A fixed line the game plays from a recording takes the recording's length. | Built 26 Oct 2026. |

## 3. The ARM moment: one order where one covers it

The drill against an anti-radiation missile (`voenizdat_osa_1991` pp. 262-264) is radiation off once the sign of a
launch is seen. Before, the NPC commander said "Излучение выключить!" and "Высокое ССЦ выключить!" together, and let the
target go in the same breath. Now:

- **«Излучение выключить!» alone.** Its task `silent` is both transmitters off: the senior operator stops the search
  radar and the operator switches the tracking transmitter off (`ssc_tx_off`, the drill of «Высокое ССЦ выключить!»),
  so the tracking transmitter's own order is not said with it.
- **The target is let go only once the transmitters are off** ("Цель сбросить!", 3 s after the order at the earliest),
  and at most every 10 s.
- **No engagement order while the silence lasts:** no new target, no method Н, no launch; each would put a transmitter
  on again (found by `helper_coverage` in NATO's DEAD raid: the commander ordered method Н against his own «Излучение
  выключить!» every 10 s, and the target was never let go).
- **ЗАПРОС between bursts.** Under the ARM threat the senior operator keeps the least emission himself (bursts of the
  search transmitter). His IFF interrogation needs the search transmitter (the interrogator's antenna is built into the
  search antenna, `mon_2112_80` p21): when ЗАПРОС waits on it between bursts he puts ВЫСОКОЕ on for it, then interrogates
  (*sam3d*: before, each waited on the other for the rest of the raid, and the search radar stayed silent).

"ARM" is said "A R M" in English (`voices.json` `say`), ПРР in full letters in Russian.

## 4. Stress, graded from what each man knows

Stress was a switch (on with jamming or a target near). It is now a level from 0 to 1 for each crewman
(`CrewMember::StressLevel`), from what **he** has heard and seen, never from the truth:

| What | Level | For |
|---|---|---|
| A war (scenario `"setting": "war"`; the air war's raids, the battles, the ARM drills) | 0.2 underneath everything | the scenario |
| «Воздух!» heard | 0.5 | 120 s |
| «Внимание!» (nearing the zone) / «Цель в зоне!» | 0.4 / 0.5 | 30 s |
| «Помеха, азимут…!», «Срыв сопровождения!» | 0.6 / 0.5 | 30 / 20 s |
| «Пуск!», «Сход!» | 0.6 | 20 s |
| "…отвернула — возможен пуск ПРР!" | 0.8 | 60 s |
| «Пуск ПРР!» (from the scope, the tracking radar's indicators or the TV) | 0.9 | 90 s |
| His own screens jammed, or a hostile within 10 km with nothing tracked | 0.7 | while it lasts |
| Our missile in flight | 0.6 | while it flies |
| An ARM bursting within 500 m of the vehicle | 1.0 | 120 s |

It rises at once and falls back with a 40 s time constant (*sam3d*). Its effects, all in proportion to the level:

- **His skill:** every time and miss chance that the tier's `stress_factor` scaled at full stress is scaled by
  `1 + (factor − 1) × level` (`TierStress`, `CrewMember::StressMultiplier`): slower, more slips, more missed lines, so
  more «Повторите!».
- **His voice:** pitch and rate up by up to `stress.pitch` / `stress.rate` steps, loudness up by up to `stress.gain`
  (`voices.json`; stressed speech rises in pitch and loudness, with large differences between speakers,
  `kirchhubel_2011`).
- **Routine talk drops:** above 0.7 he doesn't say «Выполнено!» for a drill started less than 10 s ago (the
  commander sees it done); acknowledgements still go.

## 5. The menu: Caps Lock, a radial marking menu

The crew menu was a numbered list on O. It is now a **radial menu on Caps Lock** (the project owner, 26 Oct 2026: easier
to reach than O). The lines are the catalogue's; the menu only places them.

- **Eight sectors, at most eight lines each, always in the same places** (`<system>_crew.json` `"menu"`): the Osa's
  Fire, Target, Search, Emission and threats, Readiness, March and position, Battery net, Kill signs; the PU-12M's Post,
  Regiment net (R-111), Battery, Warning, Reports, Air picture. The first place is at the top, the rest clockwise, 45°
  apart. A seat sees its own lines: the commander his orders, his net reports and the warnings; any other seat its
  reports and warnings (`MenuLines`). A test checks that every seat's every line has a place and no sector holds more
  than eight for any seat.
- **Greyed lines keep their place**, with what stops them (honest greying, `07`). A sector with none of the seat's
  lines is drawn faint.
- **The advised line** (a lesson's step, else the helper's pick) is green, and so is the sector holding it; **0** says
  it at once.
- **Mouse:** while the menu is open the mouse moves its cursor, not the head. Point at a sector and click, or flick out
  past the ring to open it; point at a line and click. **Held:** hold Caps Lock, flick to the sector, on to the line,
  let go (a marking menu: the hand learns the direction). Let go in the middle: nothing is said.
- **Keyboard, the same layout:** 1-8 the places clockwise from the top, 0 the green line, Backspace back, Esc or Caps
  Lock close.
- **Seat first (Tab, the commander):** the first ring is whom the order is said to (the crew, each seat, the battery;
  `OrderDoer` from the order's words), then what. A seat with more than eight orders pages with 9 or the wheel.
- **Quick replies at any seat:** **U** «Есть!», **I** «Выполнено!», **P** «Повторите!» (the last line said to him or the
  crew in the last 30 s is said again by its speaker, as an NPC asks).

## 6. Understanding what was said

- **Glosses** (with the helper on): under a heard line on the screen, what its abbreviations mean and an azimuth in
  thousandths as degrees ("ССЦ = the tracking radar (станция сопровождения целей); 35-00 = 210°"). Only abbreviations
  whose expansion is certain are glossed (`voices.json` `"gloss"`); СРП and АСПД are left out until a source gives them. A
  gloss explains words; it never adds what was not said.
- **What I've heard** (B, in a launcher): beside the designations heard on the net, the targets as the seat's crewman
  knows them: number, azimuth, range, height, IFF, how long ago and from what (his scope, "told by Senior operator", the
  radio). It is his own picture (`PictureOf`), first person: what reached him, never the truth.

## 7. How it sounds

- **The intercom through throat microphones.** The R-124's kit is five helmets with laryngophone headsets
  (`r124_refs`: «пяти шлемофонов с ларинготелефонной гарнитурой»). A throat microphone takes the voice off the throat:
  the mouth's highs and the hiss of the fricatives are gone, the low middle is heavy. The game filters intercom lines so
  (low-pass 2.2 kHz twice, high-pass 180 Hz, a little drive; *sam3d* estimate of the character), then the intercom's band.
- **A word said aloud in the cabin** comes from the speaker's head (spatialised at his body's head).
- **Ducking:** while a line plays, any channel with a less urgent line is turned down to about a third, so the urgent
  one is heard (the instructor counts as urgent in a lesson).
- **Stress** in the voice: §4.

## 8. NPC to NPC

- **One caller per fact** (§2): when two seats see the same thing, the second doesn't repeat it.
- **A busy crewman misses more:** a crewman who turned a hand wheel in the last half second misses a heard line half
  again as often (his tier's `miss_chance` × 1.5): his eyes and hands are on the target.

## 9. Not yet

- **Voice input** (the player speaking the order into a microphone): not built; the menu and the reply keys stand in.
- **Skill push and pull** (a skilled crewman carrying a weak one, a weak one dragging the crew down): not built; each
  crewman's tier stays his own.
- The PU-12M's stress signs (the post's own warnings) beyond the shared table, and the voices' stress tuned by ear.
