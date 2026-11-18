# Track G research: the march, battery orders, mobility, the battery area and the missiles' way (G0, October 2026)

What track G's data rests on (`docs/campaign/11_BATTERY_LIFE_PROPOSAL.md` §6, G0), where each value went, and what is
still to find. Every value is in its data file with its `source` and `confidence`; this note is the index, not a second
copy. Sources are the ids of `docs/SOURCES.md`; "sam3d estimate" and "game value" mark what is ours.

## 1. The march (G2, G3)

| What | Where it went | Source |
|---|---|---|
| The battery commander organises the march: the route, the start point beyond the column's length, control points, halts, the order of march, route cards | `nav/march.hpp` (`PlanMarch`), `battery_tactics.json` | `voenizdat_osa_1991` p239 (6.3.3) |
| A halt every 3-4 h of movement (checks, faults, fuel, rest); the plan's 3.5 h and 1 h | `battery_tactics.json` (`halt_interval_s`, `halt_interval_max_s`, `night_halt_s`) | `voenizdat_osa_1991` p238; `video_osa` agrees |
| Radio forbidden in the column on the march; signals by flags by day, lamps by night | `battery_orders.json`, `battery_orders.hpp` | `voenizdat_osa_1991` p240 |
| Spacing 25-50 m in the column (40 m taken), at least 200 m when an air attack is to be met from short halts (400 m taken) | `battery_tactics.json` | `voenizdat_osa_1991` p232; `video_osa` |
| The commandant's regulators and the movement table | `nav/movement.hpp` (the gap a sam3d estimate, 300 s) | **To find**: a Soviet regulation or textbook on the march of the period (the commandant service on routes) |
| A breakdown's chance on the march | `battery_tactics.json` (`breakdown_per_100km`, 0 until a source: G3.5's hook), `repair_s` 1800 s | sam3d estimate; G6 (on hold) brings the textbook's maintenance pp. 206-224 |

## 2. Orders to the vehicles and back (G10)

| What | Where it went | Source |
|---|---|---|
| «К маршу!», «К бою!», the read-back and «к маршу готов», «к бою готов» | `data/crew/*_crew.json`, `battery_orders.json` | `voenizdat_osa_1991` p246; `rrs_1986` (an order acknowledged by its repetition) |
| The R-123M switched on and warmed for 10 min before a call | `battery_orders.json` (`warm_s`) | `r123m_training` |
| The radiotelephonist writes every report in the journal | `battery_orders.json` (`write_s` a game value) | `pvo_bu_x_1984` art. 24 |
| The talk tables' words | `data/command/talk_tables.json` | built (P-phase research) |
| The runner, the telephone, the silence rule (twice more, then the next means) | `battery_orders.json` | game values; the order of means after `utin_2018` |

## 3. Mobility (G1)

Per class in `data/vehicles/mobility.json`: the 9A33 and the TZM from `mon_2112_80` p11, `voenizdat_osa_1991` p16 and
`korovin_tiv_2010`; the PU-12M from `valka_pu12` (82 km/h on roads, 10 in water); a man on foot from
`stroevoy_ustav_1993` art. 31 (1.44 m/s, as `data/crew/walking.json`). The ground factors off the road (woods, marsh,
built-up: G1.3) are all sam3d estimates. **To find**: the battery's trucks (the maintenance vehicle, the ATZ, the
kitchen) from a period reference; slopes for every class.

## 4. The battery area (G4)

`data/campaign/battery_area.json`: pairs 200-500 m within and up to 3 km apart, the TZMs within 500 m of their pair, the
command post at a distance that keeps control steady, a reserve position within 2 km, nothing tall within 20 m of a
launcher (`voenizdat_osa_1991` pp. 232-239); the aiming circles 30-40 m from the PU-12M and 20-50 m from a launcher
(pp. 249-250); the P-274 field cable laid at about 1.2 m/s (`ru_pu12_text`). **To find**: the field camp and its daily
routine (`pvo_bu_ix_1984`, the period's internal service regulation), the maintenance vehicle's place, the time to dig
a 9A33's emplacement with an engineer machine, the time to put up nets.

## 5. The missiles from the rear (G5)

`data/campaign/logistics.json`: six missiles on a launcher, twelve on a TZM, a launcher loaded in 5-8 min, the
technical battery's test station at 6-7 missiles an hour (`voenizdat_osa_1991` pp. 188-189). The TZM's loading at the
siding, the request rule, the regiment's answer, the train's speed and load are sam3d estimates and game values. **To
find**: the regiment's supply organisation (the technical battery's place and its loading norms), ration, water and
fuel norms (with G7).

## 6. The visible chain (G8)

The chain's posts, holders and means are `data/command/chain_pannonia.json` (P-phase research,
`docs/research/CHAIN_OF_COMMAND_RESEARCH_2026-10.md`). New: `sent_by_vehicle` in `signal_means.json`, the battery
sergeant major sent in a truck with a paper (`utin_2018` for the means; its speed and handling sam3d estimates), its
carrier's class named (`"mobility": "truck"`) so it drives the route service's roads.

## 6a. What the player's character sees (G11)

`data/gameplay/visual_range.json`: a vehicle on the ground seen 8 km by day, 3 km at dusk and 1.5 km at night, the
weather's visibility the cap, with a 10 % margin before it goes back to M1 (game values); civil twilight's 6 degrees is
the astronomical definition. **To find**: a period figure for spotting a column by eye at night (headlights, dimmed
lights, a moon), and an optic's range (the PU-12M's commander's sight, binoculars).

## 7. Conflicts

None recorded yet between the sources used. Where the textbook gives a range, the data says which value was taken and
keeps the range (`battery_area.json`'s `min` and `max`).
