# Faults that come about by themselves (roadmap C4, G6.1 in first form)

The project owner (15 Nov 2026) asked for realistic faults:
- simulated by default, with a setting to turn them off;
- tied to the later maintenance work (G6);
- with an element of probability and luck;
- simulated in certain training scenarios.

| Part | Where |
|---|---|
| The model: clocks, lives, the instruments, repairs | `simcore/faults.hpp` |
| The Osa's catalogue | `data/gameplay/osa_ak_faults.json` |
| The crew finding, reporting and mending them | `ScenarioRunner::UpdateFaults`, `OrderRepair`, `UpdateRepairs` |
| The check's steps that show them | `data/procedures/osa_ak.json`: `function_check` (deputy), `noise_check` (operator) |
| What each fault does to the vehicle | the console's fault inputs (`data/procedures/osa_ak.json`, `simcore/checks.hpp`, `outcome.fault_factors` in `data/gameplay/osa_ak.json`) |
| Tests | `simcore/tests/test_faults.cpp` |

## 1. What the sources give

The textbook (voenizdat_osa_1991, printed pages 212-221) describes the Soviet Army's single system of maintenance and
repair. It gives the following:

- **When things are done.** The system is planned and preventive. Maintenance is set by the amount of work done
  (kilometres, launches, cycles, hours of running) or by the calendar, allowing for the conditions of use.
- **Before and after use.** The crew inspects the vehicle (КО) before every fight, march, exercise or water crossing.
  It services it (ЕТО) after each of them, or every two weeks if the vehicle is not used.
- **Scheduled services.** ТО-1 and ТО-2 follow the manual's intervals. Before combat they are done whatever the
  interval. The seasonal service (СО) is twice a year. Stretching the intervals or cutting the work is forbidden.
- **Who does the work.** The vehicle's crew does КО and ЕТО. СО, ТО-1 and ТО-2 are done by the crew together with the
  maintenance vehicle's crew (МТО).
- **How long it takes.** Table 5.4 gives the man-hours of each kind of service. The year's maintenance is set at 192 h
  in a ТО-1 year and 223 h in a ТО-2 year, for no more than 110 h of the radio equipment's running.
- **Spares.** The vehicle carries its own set of spares, ЗИП-1А. The maintenance vehicle carries ЗИП-1Б, for the
  elements that ЗИП-1А does not hold, and the group set ЗИП-2 replenishes both.

It does **not** give failure rates, and no open source found does. A search for the vehicle's mean time between
failures ("наработка на отказ") found nothing. So **every rate is a game value**. Two other things are SAM3D's reading,
not the sources':
- which blocks ЗИП-1А holds, and so which faults the crew can mend;
- the instructor's fault order. The 9Ф632 trainer of pp. 201-204 simulates the air picture and the jamming, not
  equipment faults.

## 2. The model

**The clocks.** Each fault of the catalogue is one of the console's fault inputs. It ages on its own clock: the hours
its circuit runs (`runs_while`, for example the fire-control computer switched on). This follows the textbook: wear is
counted by the work done.
- **Heat.** In a hot rack the electronics age faster: twice as fast for every 10 °C over 40 °C. The rack temperature
  comes from the vehicle's climate (`vehicle_systems.hpp`, C3).
- **Maintenance.** Every clock runs 1, 2 or 4 times as fast when the vehicle's maintenance is kept, due or overdue (the
  scenario says which, until G6 keeps it).

**Luck.** Each fault's life is drawn once from the scenario's seed, as an exponential draw in units of its mean life.
The fault comes about when its clock has used that life up. The same seed always gives the same luck. A repair is a
new block, and a new block draws a new life.

**Faults there at the start.** Before the first step, the hours of work since the vehicle's last check
(`since_check_h`, 4 h unless the scenario says otherwise) are run on every clock. Any fault whose life they used up is
there from the start, and nobody knows about it. It is the function check's job to find it. Over 4,000 seeds the
model's share of faults there at the start matches 1 − exp(−hours / mean life) (test).

With the catalogue's mean lives:

| Vehicle 4 h of work past its check | Chance of a fault there at the start |
|---|---|
| maintenance kept | about 1 in 7 |
| due | about 1 in 4 |
| overdue | about 1 in 2 |

A fault coming about during an hour of work is a few percent more.

## 3. The crew finds faults only by its instruments

This follows the crew pillars: knowledge moves only through instruments and voice. Each fault names the instrument
that shows it (`found_by`):
- **The seat** that reads it.
- **What it must be set to** (`setup`): the check's switches, or the lamp's supply. The loader refuses a setup that
  names the fault's own input.
- **What the seat sees.**

While the instrument is set up and the fault is there, the seat sees it and reports «Отказ: …!». An NPC reports it at
once. A player at that seat owes the report: the helper shows it, and the crew learns of the fault only once the
player says it.

| Fault | Instrument (seat) | How it is found |
|---|---|---|
| `gyro_phase_lost` | ОТКАЗ Qc (deputy) | whenever powered |
| `hydraulic_phase_break` | ОБРЫВ ФАЗЫ ψ, γ (operator) | whenever powered |
| `scanner_phase_break` | ОБРЫВ ФАЗЫ ГОН (operator) | whenever powered |
| `srp_fault` | ОТКАЗ СРП (deputy) | with the equipment on |
| `k3_fault` | КЗ stays dark (deputy) | function check |
| `command_channel_fault` | ИП1, ИП2 at nought on ПУСК (deputy) | function check |
| `error_channel_fault`, `missile_error_fault` | the КАНАЛ Ц / КАНАЛ Р meters at nought (deputy) | function check |
| `soc_receiver_degraded`, `ssc_receiver_degraded` | КОНТРОЛЬ barely rises with ПОДЖИГ ГШ (operator) | function check (the noise check) |
| `emu_overload` | ПРИВОД βн dark, the thermal relays open (operator) | when it trips |
| `stab_limit` | ПРЕД. УГОЛ lit, and still lit after stowing (commander) | with the equipment on |
| `fuse_soc`, `fuse_ssc`, `fuse_svr`, `fuse_spk` | the system's fuse lamp lit on 0096-10M (operator) | whenever powered |

The function check now has its real steps (voenizdat_osa_1991 p159-162):
- **The deputy:** АНОД ВКЛ., КЗ, ПУСК on the static task, РЕЖИМЫ at СО Ц, Р, once the check has run its 3 min. He
  turns РОД РАБОТЫ back about 10 s later than before.
- **The operator**, at the same time: the noise check, channel by channel (p161), from the moment РОД РАБОТЫ is at ФК.
  He is free again long before the check is over.

If a target in reach breaks the check off, the noise check ends with it. A noise check still waiting once the check is
over is dropped, whether an NPC or a player holds the seat.

**A failed block is not a tripped one.** Switching the equipment off and on again resets a tripped subsystem, but it
mends nothing that has failed. So the drill master no longer plans that cycle while a prerequisite is a fault input no
seat can set right. Before this, with the fire-control computer failed, the NPC crew cycled the equipment switch without
end and the search never came up. Now the crew fights with what still works (test).

**A drive on its stop** (`stab_limit`, p47-48) switches the search off and lights ПРЕД. УГОЛ. Stowing the antenna
(ПРОХОД ПОД МОСТОМ ВКЛ.) puts the табло out only if nothing is broken, so with the drive still faulty it stays lit.
The drill master does not stow and raise the antenna to clear a trip that a fault holds; before this, the NPC commander
did so every two seconds (test). A sharp transient on rough ground, the textbook's other cause, is not modelled.

**Blown fuses** (p166: indicator lamps in 0096-9M1 and 0096-10M; p54, p101, p121, p151: dark in the function check).
A fuse in the search radar's or the tracking radar's supply stops its transmitter; one in the missile trackers' or the
command link's stops the preparation short of ПУСК ГОТОВ. The four lamps are drawn on 0096-10M; their legends and
places are SAM3D's, and 0096-9M1's twins are not drawn. The operator fits a fuse of the rated value from ЗИП-1А in
2 min, never a larger one (p222).

A fault the check does not look at stays hidden until it shows another way. A degraded receiver, for instance, shows
only as a radar that sees less far. A K3 fault met in a fight shows only as a missile that does not burst.

## 4. Repairs

**Mended by the crew.** These faults are mended from ЗИП-1А (`"repair": {"by": "crew"}`):
- the phase fuses: 5 min;
- the systems' fuses on 0096-10M: 2 min;
- the K3 coder's valve: 15 min;
- the error channel's valve and each receiver's input valve: 20 min.

While no hostile is known, the NPC commander orders the man to the oldest such fault: «…, неисправность устранить: …!».
The man is away from his drills while he works. A hostile known breaks the repair off: he goes back to his post, and
the commander orders the repair again after the fight. Once mended, the man reports «Неисправность устранена». If the
function check had found the fault, the commander has the check run again. With a player commander, the order is his
to give (`order_repair` in the crew menu's Readiness sector) and the helper shows it.

**The workshop's.** These faults need ЗИП-1Б, the maintenance vehicle's or the workshop's spares:
- the fire-control computer;
- the commands K1 and K2;
- the missile channel's error signals;
- the ЭМУ.
- a stabilisation drive (ПРЕД. УГОЛ).

The crew knows of them and fights with them (`KnownFaults`). The maintenance vehicle coming to mend them is G6.3.

## 5. Turning them off, and training

**Turning them off.**
- The missions menu's **Faults** setting is On by default. Off turns the dice off for the session
  (`ScenarioRunner::SetRandomFaults(false)`, before the first step).
- A scenario can turn them off with `"faults": {"random": false}`. The lessons and the trainer do, because what they
  teach is set by the scenario.
- Faults that a scenario sets (`own_unit.inputs`, a trigger's `set_input`) or the instructor sets (`"instructor":
  "fault"`, `"target": "<input>"`, `"to": 1` or `0`) come regardless of the setting.

**The scenario's settings.** `"faults": {"random": true, "maintenance": "kept" | "due" | "overdue", "since_check_h": 4}`.

**Training.** Two training scenarios set faults (`missions.json`):
- `osa_function_check`: the function check finds faults.
- The trainer (`osa_trainer`): the instructor puts a fault on during the drill.

## 6. Left, and how it joins G6

- **G6.1.** The clocks are kept per session. The battery's campaign should carry them, and the maintenance done,
  between missions (`FaultModel::WorkedH`). Kilometres and launches as clocks (the textbook's other measures of work)
  are not modelled yet.
- **G6.2-G6.3.** The kinds of maintenance as jobs at the vehicle's places, with the МТО's capacity. ЕТО and ТО-1 should
  reset the clocks or find faults by their own checks. The workshop's faults mended by the maintenance vehicle coming
  up (reusing the battery's breakdown helper, `Battery::Breakdown`).
- **G6.4.** Playable repair: the player opens the rack and replaces the block by the helper's steps. Today a player
  given the repair order is simply busy for its time.
- **More faults.** The missile's own faults (`ind.select`'s ОТКАЗ, already its own model), the transmitters (the
  climate's `transmitter_failed`), the chassis (G3.5's breakdowns on the march, at a fixed chance).
- **A suspicious miss.** After a missile fails to burst with nothing seen to explain it, a good commander would order
  the function check. This is not a rule yet.
- **The F9 panel.** It cannot put a fault on in the game yet; scenarios can.
