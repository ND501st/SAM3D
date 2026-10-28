# 03. Operational environments, outcomes and behaviour paths

Pillar 4: the NPC behaviour paths for every operational environment and every outcome. The structure:

1. one **crew state machine** for a single Osa that every environment shares (§1);
2. the **paths per seat** through it: what each seat does, says and waits for in each state (§2);
3. the **outcomes** of every phase and where each one leads (§3);
4. the **environments**: what each changes in the states, the paths and the outcomes (§4);
5. the **player in a seat**: how the NPC paths bend around a human (§5);
6. the **battery** (§6).

Sourced items carry their source; everything else is **sam3d** design. Callout ids (in `code`) are defined in `04` and
in `data/crew/osa_ak_crew.json`; procedure ids are in `data/procedures/osa_ak.json`.

## 1. The crew state machine (one vehicle)

The vehicle's crew is in one **readiness state** and, when engaging, one **engagement state** per target channel.

### 1.1 Readiness states

| State | Means | Entered by | Left by |
|---|---|---|---|
| R0 Cold | Everything off, hatches may be open | Scenario start, "Отбой" complete, power lost | "К бою" (R1), "К маршу" (M1) |
| R1 Powering | GTA starting, equipment switching on, warming | "К бою" from R0 | All warm-ups done (R2); fault (F) |
| R2 Ready, halted (к бою готов) | Power, equipment warm, SOC ready, АПУ free, drives on, oriented | R1 complete at a halt | Contact (E1); "К маршу" (M1); "Отбой" (R0) |
| M1 March, stowed | Driving, АПУ locked, equipment off or warm, search off | "К маршу" | Halt (R1 or R2); "Поиск в движении" (M2) |
| M2 March, searching | Driving with the SOC radiating on the GTA (`search_on_march`) | "Поиск в движении" | Contact beyond 20 km → pull off (M3); within 20 km → halt at once (R2/E) (`mon_2112_80`) |
| M3 Pulling off | Leaving the column to the side away from the target, 30–50 m (`mon_2112_80` p63–68) | Contact beyond 20 km on the march | Halted (R1/R2) |
| P Prepared position | Emplacement, external power, nets (7.6) | Scenario | Leave the emplacement (`leave_emplacement_s`), relocation |
| S Silent (EMCON) | Radars off by order; TV and the board only | "Излучение выключить" | "Излучение включить" |
| L Reloading / refuelling | TZM or fuel truck alongside; vehicle de-energised for reload (`mon_2112_80`: 5–8 min) | Empty launcher / fuel at half | Done (R2 or M1) |
| F Fault | A subsystem faulted | Equipment | Repaired (switch off and on again in order), or the vehicle is out |

### 1.2 Engagement states (per target)

| State | Means | Next on success | Next on failure |
|---|---|---|---|
| E0 Unknown | Nothing known | E1 (contact or designation) | – |
| E1 Known | A contact on the SOC or a designation from outside | E2 (designated) | Contact lost → E0 |
| E2 Designated | The tracking radar or TV is being put on it (azimuth, range, elevation) | E3 | Wrong target, nothing in the gate → E2 again; give up → E1 |
| E3 Tracking | Radar lock, TV track, mixed, or radar on a jammer | E4 | Lock lost → E2 |
| E4 Identified | IFF answered or the board says hostile; commander's decision | E5 (fire) | Friend → E9 (watch); hold → E3 |
| E5 Preparing | Missile preparation, waiting for the zone | E6 | Leaves the zone → E3; out of reach → E3 |
| E6 Launched | Missile away, guiding | E7 | Track lost → E3 (missile lost) |
| E7 Assessing | Result from the TV, the lamps, the scope | Hit → E8; miss and still in zone → E5 (re-engage); miss and out of zone → E3 / E1 |
| E8 Destroyed | Target down | Next target (E1 of the next) | – |
| E9 Not engaged | Friend, hold, out of sector | – | – |

## 2. Paths per seat

Each table: the state, what the seat does (procedure ids where the data has them), what it waits for, what it says.

### 2.1 "К бою" from cold (R0 → R1 → R2), at a halt

Source: `voenizdat_osa_1991` pp. 246–248 (the drill and its reports), times from the console data. Ideal times are the
drill master's (`06`), at the expert tier, for a cold vehicle.

| Seat | Actions (procedure) | Waits for | Says |
|---|---|---|---|
| Commander | Orders "К бою"; hatches closed (`veh.hatches`); antenna up (`apu.stow`); right R-123M on (`radio_right_on`); collects the reports; reports to the battery | Every seat's "ready" | `order_k_boyu`, `report_ready` ("к бою готов", with channels and missiles) |
| Driver | GTA start (`power_up`); generator on line when halted long (`pto_on_line`); gyro arrest held 40 s; left R-123M on (`radio_left_on`) | – | `power_on` ("Питание подано"), `seat_ready` |
| Operator | Equipment on (`equipment_on`: АППАРАТУРА ВКЛ., СИСТЕМА СТАБ. ВКЛ., TV supply); on the deputy's orders: unlock the АПУ and switch on the drives (`deploy_at_halt`) | Power (driver) | `seat_ready` ("первый готов"), `ssc_ready` ("ГОТОВ" on the range indicator) |
| Senior operator | Search on (`acquire_with_soc` or `search_on_march`): ПОИСК ВКЛ., ВЫСОКОЕ under its cover, range scale, beam | Search antenna (stabilisation 3 min), `hv_ready` (3 min) | `soc_ready` ("Готов СОЦ") |
| Deputy commander | Orders "Расстопорить АПУ", "Включить приводы"; orients the vehicle (`orient_vehicle`); checks ГОТОВ СПК; function check (`function_check`) if time allows | Power, the gyro, equipment warm | `order_unlock_apu`, `order_drives_on`, `spk_ready` ("Готов СПК"), `missiles_ready` ("СПК, СРП, ракеты готовы") |

Exit: all reports in → R2. Ideal: about 4 min including the GTA start (`mon_2112_80` p11–12: "at most 4 min"); 6.8 min
with the function check (`flak11`).

### 2.2 Search and detection (R2/M2, E0 → E1)

| Seat | Does | Says |
|---|---|---|
| Senior operator | Watches the scope; chooses the range scale (0–35 working, 0–15 close: ePap) and the beam programme (I or I–II for low targets: `voenizdat_osa_1991` p250); MTI on below 35 km for low targets | `contact` on every new echo held two sweeps: "Цель, азимут 35, дальность 24, луч один"; `contact_lost` |
| Commander | Listens to the PU-12; passes designations on; asks for IFF; decides which target | `designation` ("Цель номер 6201, азимут 05-40, дальность 24, высота 10"), `order_iff`, `order_take_target` |
| Deputy | Watches the TV in the target's direction once the azimuth is on it | – |
| Operator, driver | – (driver keeps driving in M2) | – |

### 2.3 Designation and capture (E1 → E2 → E3)

| Seat | Does | Waits for | Says |
|---|---|---|---|
| Commander | Orders "Цель номер …, взять!" (take it) | E1 | `order_take_target` |
| Senior operator | Azimuth lock open, wheel onto the target's mark until the azimuth line covers it (`acquire_with_soc` then wheel work) | The order (or his own contact) | `az_on` ("Азимут на цели") |
| Deputy | ЦУ (auto acquisition: the antenna scans the beam in elevation), or the elevation wheel onto the target (`ready_capture`) | `az_on` | `order_capture` |
| Operator | Tracking transmitter on (`capture_with_ssc`), range wheel onto the target's range | `az_on`; `hv_ready` | `range_on`, then `captured` ("Захват!") or `capture_failed` |
| All | Low target (НЛЦ lit): deputy calls it; switch to TV angles (`track_low_target`) | – | `too_low` ("НЛЦ") |

Branches: radar capture; TV (deputy ПА, `tv_track`); ПОЛУАВТОМАТ (operator, `track_range_semi_auto`); radar on a jammer
(`on_jammer`, 9.1). The NPC commander orders the TV (`order_low_target`) on НЛЦ, whether the radar holds the target, has
lost it, or never had it (the search radar may have lost it too: the TV looks along the azimuth line), and ПОЛУАВТОМАТ
(`order_semi_auto`) once the TV holds it with the tracking transmitter on; with nothing in the air a lost lock frees the
deputy from his launch drill. Mission: `osa_low_flier_em.json`. Ideal (network chart, `voenizdat_osa_1991` Fig. 7.2): the tracking radar on the target 10 s,
capture 6–7 s.

### 2.4 Identification and decision (E3 → E4)

| Seat | Does | Says |
|---|---|---|
| Senior operator | ЗАПРОС (or ЗАПРОС К) on each new contact within the interrogator's reach, by himself or on `order_iff` ("Запрос!") | `iff_friend` ("Свой") / `iff_no_reply` ("Ответа нет") |
| Commander | Decides from the IFF, the board's code (friend 1x, practice 7x, enemy 8x), the orders and the zone; an unidentified target first gets `order_iff`; a friend under track is held and let go; a miss with the target still in the zone gets a second missile | `fire_permitted` ("Пуск разрешаю") / `order_second_missile` ("Вторая — пуск!") / `hold_fire` ("Огонь не открывать") / `order_drop_target` ("Цель сбросить!") |

### 2.5 Preparation, launch, guidance, assessment (E4 → E8)

| Seat | Does | Waits for | Says |
|---|---|---|---|
| Deputy | БР, key in (`prepare_firing`); preparation starts at ЦЕЛЬ В ЗОНЕ; key to ПУСК, ПУСК (`launch`) | ВНИМАНИЕ, ЦЕЛЬ В ЗОНЕ, ПУСК ГОТОВ, permission | `target_near_zone` ("Внимание"), `target_in_zone` ("Цель в зоне"), `missile_ready` ("Ракета готова"), `launch` ("Пуск!"), `missile_away` ("Сход") |
| Operator | Holds range (automatic in АВТОМАТ; by hand otherwise); watches for the missile's response on the range indicator | – | `lock_lost` if it happens |
| Senior operator | Keeps watching the scope for new contacts (the second target) | – | `contact` |
| Deputy | Watches the TV to impact | Flight time | `hit` ("Цель поражена") / `miss` ("Промах") |
| Commander | Second missile (salvo, ≥ 4 s: ePap) or re-engage while ЦЕЛЬ В ЗОНЕ stays lit (`voenizdat_osa_1991` p257); next target | Result | `order_second_missile`, `order_next_target`, `report_result` (to the battery) |

Ideal: computer 3 s, missile preparation 15 s, launch 1.2 s, salvo interval 4 s, watching the result 10 s (network
chart); detection to launch 26–40 s (`nvo_2015`, `korovin_tiv_2010`).

### 2.6 March, halt and stow (M1, M2, M3, "Отбой")

| Seat | Does | Says |
|---|---|---|
| Commander | "К маршу" / "Стой" / "Вперёд"; route; when to search on the move; lowers the antenna for bridges (`lower_search_antenna`) | `order_march`, `order_halt`, `order_go`, `order_search_on_march` |
| Driver | Drives; halts; reports | `halted`, `ready_to_march` ("К маршу готов", `voenizdat_osa_1991`) |
| Operator | Stows the SSC and locks the АПУ (`stow_ssc`) before the vehicle moves | `apu_locked` |
| Deputy | Stows the launch console and TV (`stow_launch_console`) | `seat_ready` |
| Senior operator | Stops the search (`stop_search`) unless searching on the march | `seat_ready` |
| Driver (after stow) | Generator off line (`pto_off_line`) | – |

### 2.7 Reload and refuel (L)

| Seat | Does | Says |
|---|---|---|
| Deputy | Reports the launcher empty (lamps) | `launcher_empty` ("Ракет нет") |
| Commander | Calls the TZM on the battery net (`reload`); orders the vehicle de-energised for reloading (`mon_2112_80`) | `order_reload`, `request_tzm` |
| Driver | Reports fuel at half (ePap); calls the fuel truck through the commander | `fuel_half` |
| TZM crew | Drive alongside, load from stock (`battery.hpp`, `launcher.hpp`: one missile a minute) | `tzm_alongside`, `reload_done` |

## 3. Outcomes catalogue

Every phase's possible outcomes, what they look like to the crew, and the path they lead to.

| Phase | Outcome | Seen by (instrument) | Path |
|---|---|---|---|
| Power | GTA starts / fails to start | Driver (lamps) | R1 / restart; generator instead (halted) |
| Power | Fuel runs low / out (`fuel_power_cut`) | Driver (gauge) | Refuel (L); out → R0 |
| Equipment | Warm-up done / fault (wrong order, prerequisite lost) | Operator, everyone (lamps) | Next step / switch off and on in order (F) |
| Equipment | Function check passes (3 min) | Deputy | Done; the debrief records it |
| Orientation | Synced / not synced | Deputy (`heading_synced`) | Board azimuths right / wrong |
| Detection | Contact | Senior op | E1 |
| Detection | Nothing (masked by terrain, below the radar's horizon, in clutter with MTI off, beyond range) | Senior op (empty scope) | Beam programme I / I–II, MTI on, range scale, ask the board, move (M7) |
| Detection | Ghost (chaff, false targets) | Senior op | Capture fails or "no target in the gate"; ignore after the second try |
| Detection | Jamming strobe | Senior op | Report `jamming`; board / EWR for position; 9.1 path |
| Designation | Az / range on | Senior op, operator | E2 → capture |
| Capture | Captured | Operator, deputy (ЗАХВАТ) | E3 |
| Capture | Not captured (wrong range, wrong beam, too far, too low: НЛЦ, clutter: СДЦ off) | Operator | Fix and retry; TV; MTI on; wait to close |
| Capture | Captured a ghost / the wrong target | Operator (no ЗАХВАТ: a ghost never captures) | Retry |
| Tracking | Lock lost (masked, clutter, crossing too fast, RGPO walk-off, jammed, target down) | Operator | Re-acquire; jammer path; TV |
| Tracking | Range lost in jamming, angles held | Operator (range indicator), deputy | `fire_at_jammer`: range by hand, ПОМЕХА |
| Tracking | A target-bound drill left waiting with nothing tracked and nothing in the air (method Н on a jammer whose track is gone, the TV engagement on a target lost in the haze) | Commander | After 10 s he lets the deputy's drill go (`NpcReleaseStaleDrills`), so the next "take the target" reaches a free seat; before 26 Oct 2026 a busy deputy blocked every capture for the rest of a raid |
| Identification | Friend / no reply | Senior op | E9 / E4 |
| Decision | Fire / hold / drop | Commander | E5 / E3 / E1 |
| Zone | Near zone (ВНИМАНИЕ), in zone, never in zone (passes outside: crossing parameter, height, speed) | Deputy | E5 / wait / drop; move (M7) |
| Launch | Launched / blocked (why: hold, no missile, hatches, key, mode) | Deputy (lamps; the debrief names the reason) | E6 / fix the cause |
| Guidance | Track held / lost | Operator, deputy | E7 / missile lost |
| Result | Hit, miss, out of reach (flight time ran out), target already down | Deputy (TV) | E8 / re-engage / next |
| Evasion | Target breaks, dives, jinks on the lock or the launch (`target_behaviour.hpp`) | Senior op, deputy | Hold the track; expect a lower chance; re-engage |
| Ammunition | Pair spent, launcher empty | Deputy | Next pair (preparation) / reload (L) |
| C2 | Designation received / lost / source silent | Commander | Pass on / switch source / autonomous search |
| Radio | Order shouted (box off) | Commander | Delay (`crew_comms.hpp`) |

## 4. Environments

Each environment changes which states are possible, which branches are preferred and what goes wrong. **Paths not
listed are the defaults of §2.**

| # | Environment | What changes | Preferred branches | Typical outcomes |
|---|---|---|---|---|
| V1 | Garrison / park (cold, hatches open) | Starts at R0; all of "К бою" | §2.1 in full; function check | Times against the norm |
| V2 | March in a column (`battery.hpp`) | M1/M2; GTA only; no capture before a full stop (`voenizdat_osa_1991` p250: unlock and slew the АПУ only after a full stop) | Search on the march; beyond 20 km pull off, within 20 km stop at once | Late detection; the short halt |
| V3 | Short halt | From M2 to firing without full deployment of the battery | Fast R2 (already powered) | 35–50 s deploy (`video_osa`, unverified) vs 4 min norm |
| V4 | Prepared position (7.6) | External power, emplacement, nets, dummy positions; leaving takes time | Generator off; decoys | Strike on the position → relocate |
| V5 | Combat formation in pairs | Neighbour's radar as a source (`share_picture`); sectors | Divide targets with the pair | Saturation |
| V6 | Ambush (prepared points, roving) | Silent until the target is close (the NPC commander springs it when a hostile is designated inside 15 km); move between points | S (silent, task `ready_silent`) then short radiation; TV | Pop-up targets |
| V7 | River crossing | Afloat at 8 km/h; wait 2 km short; cross in pairs | No firing afloat (sam3d: not sourced either way) | Exposure during the crossing |
| V8 | Night / poor visibility | The TV is useless; driving by the PNV | Radar only; no TV branches | Low targets harder |
| V9 | Low targets (below ε 00-30) | НЛЦ; Горка; clutter | Beam I / I–II, MTI (`order_mti` when a low target is designated), TV angles + radar range (`track_low_target`) | НЛЦ, clutter losses |
| V10 | Helicopters (hovering, pop-up) | Optical zone Ф (≤ 100 m/s, 10–25 m, 2–6.5 km); pop-ups behind cover (`target_behaviour.hpp`) | TV; short windows | Exposed for seconds; masked |
| V11 | Jamming: noise on the SSC | Range lost; angles on the jammer (9.1) | `fire_at_jammer`, range by hand | Burn-through at a few km |
| V12 | Jamming: noise on the SOC | Strobes, no blips | Board / EWR positions; TV | Late detection |
| V13 | Jamming: RGPO, chaff, false targets | Gate walked off; ghosts | Retry capture; external-coherence MTI; ignore ghosts | Lost locks |
| V14 | EMCON (radars silent; the battery's `emcon` order or the regime `silent`, `16_EMISSION.md`) | No SOC or SSC | `ready_silent`; `order_emcon_tv` (`emcon_tv_azimuth`, `emcon_tv_engagement`): board azimuth led on, a TV search across the designation, ПОМЕХА; the key turned when the target fills the crosshair | Range by the board; short windows; a designation a minute old |
| V15 | Saturation (several targets) | Priorities; two channels; salvo or single shots | Threat order (time to the defended point, type) | Missed targets; fast reload |
| V16 | Friendly aircraft about | IFF matters; fire discipline | ЗАПРОС on every contact; hold on a reply | Fratricide is a crew mistake (debrief) |
| V17 | C2 lost (PU-12 destroyed, radio jammed) | No designations | Switch source; autonomous search | Late detection |
| V18 | Equipment faults | A subsystem out | Channel fault → КАНАЛЫ to the good channel, single missiles (`voenizdat_osa_1991`); power fault → generator | Reduced capability |
| V19 | Out of missiles / fuel | L | TZM / fuel truck (`battery.hpp`) | Down time |
| V20 | Under threat of strike (anti-radiation weapons, a strike on the position; roadmap E6, `docs/EM_ARCHITECTURE.md` §35) | Radiating is dangerous: SEAD aircraft fire ARMs on what their warning receivers hear (`osa_arm_drill.json`, `osa_arm_drill_shrike.json`) | Least emission (the regime `arm` from the battery commander or the regiment, told to the crew by the commander with its turns and seconds, `16_EMISSION.md`; `voenizdat_osa_1991` p262): the senior operator's own two-turn bursts (`hv_on`, `hv_off`, ЛУЧ АВТ. I–II by `arm_beams`), the SSC on from Table 7.3's range, off after the burst (`order_ssc_off`, `ssc_tx_off`). The signs (senior operator, class2 and up): "Пуск ПРР! Азимут …" (a small fast mark at zero parameter), "Цель … отвернула — возможен пуск ПРР!" (warned of ARMs); the commander, on hearing it, lets the missile in the air burst, then «Излучение выключить!» (`order_emcon_off`, `order_ssc_off`, the target dropped) for `arm_silence_s` and "21: пуск ПРР, азимут …" up the battery net; the regiment's standing order (`relocate_after_arm`): stow and change position when the fight is over (p238) | A HARM-class missile misses by tens to hundreds of metres if the radar goes silent early, hits if late; a Shrike-class one falls kilometres off; a dummy's decoy radiating while the radar is silent takes it; an untrained senior operator: hit |

## 5. The player in a seat

The NPCs' paths don't assume an NPC in the other seats. When the player holds a seat:

1. **NPCs wait for the player's part** exactly as they would for an NPC's (the condition isn't met).
2. **They remind.** The seat that waits calls it after its patience runs out (skill: `05`), naming what it needs:
   "Оператор, передатчик!" (`remind`), then the commander orders it by name. In training the overlay shows the player
   the control (`08`).
3. **They react to the player's mistakes as to an NPC's.** An out-of-order action is a deviation in the debrief; an
   unsafe one (high voltage before the warm-up: a fault) is reported by whoever sees the lamp; the commander orders the
   fix (switch off, wait, on again).
4. **They report to the player when he commands.** Every report goes to the commander's seat; a player commander sees
   the crew's picture as what was said (`01` §4), and an order he gives is acknowledged (`ack`) or refused with the
   reason (`cannot`: "can't, the АПУ is locked").
5. **They never do the player's part for him.** If the player never acts, the chain stops there and the debrief says
   so.

## 6. The battery

The battery commander (PU-12M) commands four vehicles (`battery.hpp`, `07` §3). Per vehicle, the commander's paths
above apply; above them:

| Battery state | Battery commander | Vehicle commanders | Others |
|---|---|---|---|
| March in column | Route, spacing (40 m, 400 m under air threat), halts | Keep station; search on the march by order | TZM and fuel truck in the column |
| Short halt | Orders the halt; designates | Halt, deploy, engage | – |
| Combat formation | Pairs, covered unit, axis; sectors | Deploy at the slot | TZM 500 m behind its pair |
| Ambush / river crossing / bounding | The drill's orders (`battery.hpp`) | Move by pairs; hold fire until ordered | – |
| Engagement | Fire distribution: which vehicle takes which target (by sector, readiness, missiles left); fire permission; cease fire | Report readiness, capture, launch, result, missiles left | The plotter keeps the board |
| Reload / refuel | Sends the TZM / fuel truck | Report empty / fuel at half | TZM crew loads |
| C2 loss | – | Switch source; act on standing orders (sector, free fire or hold) | – |

The battery's NPC behaviour reuses the vehicle-level machinery: each vehicle has its own crew (NPC) whose commander
takes the battery commander's orders the way a vehicle crew takes its commander's.
