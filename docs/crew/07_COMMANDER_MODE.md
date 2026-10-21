# 07. In command: the player gives the orders

The second half of the game (pillar 2): the player is in charge of the team, first the single Osa as its commander,
then the whole battery as the battery commander in the PU-12M.

## 1. Principles

1. **The player commands, the crew does.** Every order goes through the same path as a scenario trigger's
   (`ScenarioRunner::GiveOrder`, on the R-124 or shouted), is acknowledged or refused with a reason, and is carried out
   by the NPCs at their skill.
2. **The player knows what his crew tells him.** The commander's HUD shows the crew's picture (`01` §4): the contacts
   the senior operator called, the designations from the PU-12, the lamps the deputy reported. Not the truth.
3. **The order menu is honest.** Orders that can't be carried out now are shown greyed with the reason the drill master
   gives (the first unmet condition: "the АПУ is locked: halt first").
4. **He may advise from the drill master too**, at the assistance level he chose (`08`): "next best order".

## 2. The vehicle commander's order menu

Grouped as the commander thinks, each entry an order of `04` §3:

| Group | Orders |
|---|---|
| Readiness | К бою; Отбой; Функциональный контроль; Ориентировать машину; Радиостанции включить |
| Movement | К маршу; Вперёд; Стой; route and position (map); Антенну опустить / поднять; Смена позиции |
| Power | Включить ГТА; Генератор на линию; Выключить двигатель |
| Search | Поиск; Поиск в движении; Прекратить поиск; Луч I / II / III / I–II / I–III; СДЦ; range scale |
| Target | Designation (pick a contact or a board mark); Цель — взять; Запрос; Сопровождать по ТВ; Низколетящая; Полуавтомат; Цель сбросить |
| Fire | Приготовиться к пуску; Пуск разрешаю; Пуск запрещаю; Вторая — пуск; Прекратить огонь; По постановщику — метод Н |
| Emissions | Излучение выключить / включить; Работать без излучения |
| Logistics | Перезарядка; Заправка |
| Command | Перейти на {source} (C2 sources); report to the battery |
| Any | Отставить (cancel the last order) |

Selecting a target and an order together ("this contact — take it") is one action.

## 3. The battery commander

The battery commander (PU-12M, `docs/PU12_DEVELOPMENT.md`) commands four vehicles plus the TZM and the fuel truck. His
orders go on the battery net to the vehicle commanders (NPCs), who turn them into their own crews' orders.

| Group | Orders | Built on |
|---|---|---|
| Movement | March (route), short halt, combat formation (covered unit, axis), halt, resume, ambush (points, roving), river crossing, far bank held, bounding | `battery.hpp` orders |
| Readiness | К бою (all or one vehicle); Отбой; readiness reports | vehicle orders |
| Air picture | Designations (from the board) to a vehicle or all; air threat | `pu12.hpp`, `c2_sources.hpp` |
| Fire distribution | Assign a target to a vehicle; sectors of fire; fire permission / hold / cease fire for one or all | new (phase 8.9) |
| Emissions | Battery EMCON; which vehicles radiate | vehicle orders |
| Logistics | Send the TZM / fuel truck to a vehicle | `battery.hpp` reload, refuel |

The progression (pillar 2): the player first commands one vehicle with NPC crew; when qualified (`08` §4) he commands
the battery, where his vehicle commanders are NPCs with their own skill.

## 4. What NPC commanders do

When the commander is an NPC (the player in a working seat, or the other vehicles of the battery), the **NPC commander**
runs the same menu by doctrine: it reacts to the crew's reports and the radio, issues the orders of `03`'s paths, and
asks the drill master for the best decision where a choice is open (`06` §3.6). Its skill (judgement, communication)
decides how quickly and how well.
