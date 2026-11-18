# 16 — The emission regime: decided above, passed down by voice, kept by the radar seats

Written 27 October 2026, at the project owner's request: the battery commander decides the emission regime (EMCON) or
passes on the regiment's; the launchers' commanders hear it on the battery net, tell their crews before they deploy or as
soon as it changes, and remind the radar seats how many antenna turns on and off, and how many seconds that is. It builds
on `04_COMMANDS_AND_CALLOUTS.md` (what is said), `12_COMMUNICATIONS.md` (who hears it), `14_HELPER.md` (the duties a
player owes) and `15_TALK.md` (turn-taking). Where a rule is ours it is marked *sam3d*.

## 1. What the sources say

| Fact | Source |
|---|---|
| The battery commander's decision fixes, among other things, each launcher's sector and order of search and fire and **the measures against jamming and anti-radiation missiles**; he gives it as an oral combat order. | `voenizdat_osa_1991` p243 |
| Under the threat of ARMs, the least emission: the search transmitter's high voltage on for **2 antenna turns after every 3-4**; the TV for tracking; the tracking transmitter on only from Table 7.3's range (24 / 20 km for a meeting at the far edge, fast / slow target), off after the burst. | `voenizdat_osa_1991` pp262-263 |
| Wartime frequencies are freed by a district signal at higher readiness; in peacetime the radars radiate only on permitted frequencies (and the tracking radar in permitted directions). | `voenizdat_osa_1991` pp208-211 (§5.1.3) |
| On the march the battery's radios are silent except for fire control. | `voenizdat_osa_1991` p240 |
| The search radar on for only two turns at a time (a Czechoslovak operator's account). | `vr_1985_plss`, `video_osa` |

The research notes: `docs/research/OSA_MANUALS_2026-10.md` (chapters 5-7), `OSA_RESEARCH_2026-10.md` (who sets the
regime), `CREW_COMMS_2026-10.md` (the gun batteries' BU-IX, for comparison only). No source gives the words a launcher's
commander uses to pass the regime on, nor how often he reminds his crew: those are *sam3d*.

## 2. The regimes

`data/command/emission_regimes.json`, `simcore/emission_regime.hpp` (`EmissionKind`, `EmissionRegime`, `EmissionDef`).

| Regime | The search transmitter | The tracking transmitter | Who keeps it |
|---|---|---|---|
| `free` | as the fight needs | as the fight needs | – |
| `limited` | bursts: on 2 turns, off 6 (*estimate*), while nothing is tracked and no hostile is within the tracking radar's reach | as the fight needs | the commander's ВЫСОКОЕ on / off, the first burst said aloud, then a standing order (`04` §3) |
| `arm` | bursts: on 2 turns after every 3 (p262); off while the tracking radar holds a stable track alone | from Table 7.3's range; off after the burst | the senior operator himself (`ArmEmission`); the commander for the tracking transmitter |
| `silent` | off | off | «Излучение выключить!»; the TV and the battery's designations |

A regime's turns can be given in the order (`on_turns`, `off_turns`); the seconds are said at the antenna's own rate
(33 rpm: a turn is 1.8 s, so 2 turns are 4 s and 3 turns 5 s). The older doctrine forms still load: `search_bursts`
{on_s, off_s} is `limited` in seconds, `arm_threat` (with `arm_search_off_turns`) is `arm`.

## 3. The chain

Each level acts only on what reached it (pillar 3: knowledge moves through instruments and voice).

1. **The regiment** orders a regime on its command net (the R-111 at the PU-12M): a ПТ-1 group, 631-634 (*sam3d*
   groups, like every group in `talk_tables.json`), said twice, again every `repeat_s` until the post hears it (up to
   `repeats` times), decoded by whoever hears it.
2. **The battery commander** decides the regime himself or passes the regiment's on after `relay_s` (*estimate*); when a
   launcher reports an ARM attack on the battery net he puts the battery under the least emission after
   `arm_on_report_s` (unless it is under it, or silent, already). He says it on the battery net as a circular order,
   twice, no receipt (`rrs_1986`).
   - At the PU-12M (`ScenarioRunner::UpdatePostEmission`) an NPC battery commander orders it
     (`order_battery_emission_<regime>`); a player decides, and the helper shows the regime he owes until he orders one.
   - With an Osa scenario's own battery (`battery_commander: "npc"`, `UpdateOsaBatteryEmission`) the abstract battery
     commander does the same and repeats an order the Osa did not hear (its commander's set off, jammed, off the net).
   - In one battery (`battery_link.hpp`) the post's order reaches the Osa's set, and the Osa's report of an ARM attack
     reaches the post.
3. **The launcher's commander** hears it (`CommanderRegime()`): the regime he orders the tracking transmitter by (Table
   7.3 under `arm`), and keeps silent by. He tells his crew (`order_emission_regime`) `pass_on_s` after he hears it
   (*estimate*), with the regime's words and its bursts in turns and seconds: «Режим излучения — минимальный, угроза ПРР:
   высокое на 2 оборота (4 секунды), через 3 оборота (5 секунд)!». The senior operator reads the bursts back
   (`emission_readback`); under `arm` the operator is then told the tracking transmitter's rule (`order_remind_ssc`:
   not before 20 km, 24 for a fast target, off after the burst).
4. **The crew** keeps the regime it was told (`CrewRegime()`), never the one the commander merely heard: until he says
   it the senior operator radiates as before. A commander a script stands in for (no crew data, or no NPC or player at
   his seat) passes nothing on, and the crew goes by what he knows.

**Before deploying.** The regime in force at the start is the combat order's: the doctrine's `emission`, or a battery
order of the regime at 0 s (`emcon`, `emission`). The commander knows it before the battery deploys and tells his crew at
his first think, before the radars come on (`test_emission_regime.cpp`).

**Reminders.** Under a regime of bursts the commander reminds the radar seats (`order_remind_bursts`, and under `arm`
`order_remind_ssc`) when he learns of a hostile after `remind_quiet_s` without one while the bursts still apply (it is
far out), and, under `arm` only, when a burst runs longer than the regime's by `breach_turns` and a few seconds' grace.
Under `limited` he orders each burst himself, so his own «Высокое выключить!» is the correction. A breach counts only
once the crew has kept one burst of the regime (the first burst is the search drill's), and is recorded whoever
commands. The reminders go on the intercom at priority 2, behind the fight's orders. *sam3d*: no source says when a
commander reminds his crew; these are the moments a reminder would matter.

**A capture holds the burst.** Once the commander orders a target taken («Цель ... — взять!»), the search transmitter
stays on until the tracking radar has it or he lets it go, for at most 30 s: the target designation is the search
radar's picture (`ScenarioRunner::CaptureUnderWay`, *sam3d*). Under `arm` the senior operator doesn't switch it off
between bursts meanwhile, under `limited` the commander doesn't order it off, and the time is no breach.

**ЗАПРОС between bursts.** The interrogator's antenna is built into the search antenna, so the senior operator's
ЗАПРОС of an echo seen in the last burst needs the search transmitter on: under either regime he puts ВЫСОКОЕ on for it
and interrogates (`ArmEmission`, `QueryBurst`), and the commander's next burst order doesn't cut his drill short
(*sam3d*: otherwise the query, and the orders behind it, waited on the next burst).

**An ARM's sign.** The commander who hears a sign of an ARM launch (the senior operator's «Пуск ПРР!» or the turn away,
the tracking radar's or the TV's) silences the vehicle once nothing of ours is in the air, and reports the attack on
the battery net (`report_arm_attack`); the battery commander puts the battery under the least emission. A player
commander heard it too: with the helper on, the silence and the report are shown as his duties, and his report from the
crew menu (chest switch at ПРД) reaches the battery commander as the NPC's does.

## 4. The player

- **The Osa's commander**: the crew menu's sector "Battery and its orders" has `order_emission_regime`,
  `order_remind_bursts` and `order_remind_ssc`. The reminders are greyed while the crew has no regime of bursts. With the
  helper on, passing a new regime on is shown as his duty, and so are the reminders when they fall due, under the
  limited regime every burst's «Высокое выключить!» and «Высокое включить!», and after an ARM's sign the silence and
  `report_arm_attack` (the radio sector). While the search runs in bursts the readiness drills leave the search
  transmitter to the bursts.
- **The senior operator**: under `arm` he keeps the bursts by hand (the helper shows each ВЫСОКОЕ on and off, and
  ВЫСОКОЕ on through a capture); under `limited` he is ordered each burst.
- **The PU-12M's battery commander**: the sector "The battery's emission" has the four `order_battery_emission_*`; each
  needs a way onto the battery net (his box on its set, the set transmitting) and is greyed on the march.
  `ScenarioRunner::CommanderOrderEmission` is the same order from code (Unreal's menu).

## 5. Scenario keys

```jsonc
"doctrine": { "emission": { "regime": "limited", "on_turns": 2, "off_turns": 6, "from": "battery" } },  // the combat order's
"emission_orders": [ { "at_s": 60, "regime": "arm", "from": "regiment", "notes": "SEAD expected" } ],   // during the mission
"battery": { "orders": [ { "at_s": 0, "order": "emission", "regime": "silent" } ] }                     // the battery's own script
```

`emission_orders` need a battery or a post, `at_s` > 0, and at a post a regiment's order needs the radio data's net
`regiment`. The mission `osa_emission_orders.json` (the missions menu) plays the chain through: the battery commander's
bursts, the regiment's least emission a minute in, then a SEAD pair with Shrike-class missiles (the ARMs p262's
measures were written against: a radar that falls silent is lost; a HARM-class missile flies on to where it last heard
it, so silence late in its flight is luck).

## 6. The record

`ScenarioRunner::GetEmissionRecord()`: regimes heard and told, how long after hearing the crew was told, the seconds at the
halt under bursts and the seconds the search transmitter radiated in them, the long bursts and the reminders. The
debrief ends with a line of it. The air war's raids give the regime in the combat order (`raid_scenario.hpp`: the
regiment's least emission once ARMs are expected, else the battery's bursts by its posture).

## 7. Where it lives

| Part | Code | Data | Tests |
|---|---|---|---|
| Regimes, words | `simcore/emission_regime.hpp` | `data/command/emission_regimes.json`, `talk_tables.json` 631-634 | `test_emission_regime.cpp` |
| Scenario keys | `scenario.hpp` `LoadEmissionRegime`, `LoadEmissionOrders`, `LoadDoctrine` | `data/scenarios/osa_emission_orders.json` | `test_emission_regime.cpp` |
| The battery's regime | `battery.hpp` `BatteryOrder::Kind::Emission`, `Battery::Regime()` | – | `test_emission_regime.cpp`, `test_crew_systems.cpp` |
| The chain | `ScenarioRunner::UpdateEmission`, `UpdateOsaBatteryEmission`, `UpdatePostEmission`, `OrderBatteryEmission`, `HearBatteryEmissionOrder` | – | `test_emission_regime.cpp` |
| The crew | `ScenarioRunner::NpcPassOnRegime`, `TellRegime`, `NpcRemindRegime`, `ArmEmission`, `NpcBurstOff` | `osa_ak_crew.json`, `pu12m_crew.json` | `test_emission_regime.cpp`, `test_arm_drill.cpp` |
| One battery | `battery_link.hpp` `Sync` | – | `test_emission_regime.cpp` |

## 8. Not yet

- Peacetime limits (§5.1.3): permitted frequencies and directions, the dummy loads for maintenance. The letters
  (frequencies) are not modelled per vehicle.
- The regime's sectors: p243 gives each launcher a sector of search; the regime is battery-wide here.
- The regiment's order on the ASPD-U data link (the post's telecode) as well as by voice.
- The emission of the radar cabled to the post (the P-19 at 6 or 12 rpm) under the regime: the regime is the launchers'.
