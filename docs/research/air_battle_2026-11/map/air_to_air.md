# Map: air-to-air warfare, our own aircraft, IFF (SAM3D, worktree C:\dev\SAM3D-wt\air-battle, commit 1d09489)

Read-only survey for the owner's request of 15 Nov 2026 (the enemy's air war in its hot phase, and air-to-air warfare
between our and the enemy's aircraft). All paths are relative to `C:\dev\SAM3D-wt\air-battle`. Line numbers are at
commit 1d09489.

**Short answer.** No air-to-air engagement is simulated anywhere, not even one aircraft against another in the abstract.
"Our fighters" is a single number per front (`FrontDef::friendly_fighters`, 0..1). The enemy's planner reads it to size
escorts, and the resolver reads it as a flat per-aircraft loss chance on the main flights of a package. Escorts exist as
a flight role and fly the strike's route, but they fight nothing. Their only effect is to cut that loss chance to 30 %.
No aircraft type carries a radar, an air-to-air missile, an air-to-air gun load or a g limit. `air_to_air` (0..1) is
loaded for each type but never read. Our side does have aircraft as data: the order of battle (three fighter regiments
of MiG-21s and MiG-23MLs, Su-22s, L-39s, transports, helicopters), basic models in our colours, and authored "friend"
targets in five scenarios. In the dynamic-campaign testbed our flights fly CAP orbits and intercept tracks, but they
interact with nothing. IFF is fully modelled for the player's battery as a binary transponder: the Osa's ЗАПРОС, ЗАПРОС К
and КЛАПАН, the PU-12M's cabled-radar replies and its ДХ keypad, and the NPC drill and its "hold fire". There are no
codes, modes or code changes. Roadmap 17.2 ("Our fighters") promises the friendly corridor, IFF codes and their
changes, the fighter regiment's notices through the pipeline, and friendly fire when a notice or the codes did not
arrive. Our air force as a planner is explicitly not planned. Nothing on the roadmap promises enemy-vs-our-fighter
combat, AAMs, BVR or dogfighting.

---

## 1. The enemy air command's fighter-related model (`simcore/include/simcore/air_war/`)

### 1.1 Flight roles: `types.hpp:177-193`

```cpp
enum class FlightRole {
    Strike, Dead, Sead, Hunter,
    Escort,      // fighter escort / clearing the airspace
    Jammer, Recce, Decoy, Helicopter, Missile,
    Awacs,       // airborne early warning and control (E-3, A-50): the defender's fighters met warned and vectored
    Tanker, Elint, Count };
```

- No `Cap`, `Sweep`, `Intercept`, `Fighter` or `AirSuperiority` role exists. "Clearing the airspace" (the Pact's
  расчистка воздуха group) is folded into `Escort`.
- `FlightRoleName` (`types.hpp:195-212`) gives the names used in logs and saves.

### 1.2 Data: our fighter threat, escorts and AWACS (`data.hpp`)

| Field | Where | What it is |
|---|---|---|
| `FrontDef::friendly_fighters` | `data.hpp:158` | "the player's side's fighter threat over this front, 0..1 (escort need)". A constant per front. Loaded at `data.hpp:1168-1171` (range 0..1). |
| `AircraftType::air_to_air` | `data.hpp:355` | "value as an escort, 0..1". Parsed at `data.hpp:1361`. **Never read by any code** (grep over `simcore/`, `prototype/`, `tools/` and `unreal/`: only the loader). |
| `AircraftType::max_turn_deg_s`, `evasion`, `rwr_em`, `self_jammer_em` | `data.hpp:327-333, 356` | A fixed turn rate, a 0..1 survival factor, and EM receiver and jammer ids. No radar, AAM, gun, g limit, roll rate, fuel or IR signature fields. |
| `PackageTemplate::escort_ratio` | `data.hpp:407` | "escort aircraft per main aircraft, scaled by the front's fighter threat". Loaded at `data.hpp:1546`. |
| `PackageTemplate::escort_jamming` | `data.hpp:409` | Jammers fly with the package instead of standing off. This is EW, not fighters. |
| tuning `attrition.escort_fighter_kill` = 0.03 | `data.hpp:747`; `data/gameplay/air_war.json:15` | The base per-aircraft chance that our fighters bring down a main-flight aircraft. |
| tuning `support.awacs_fighter_cut` = 0.5 | `data.hpp:816`; `data/gameplay/air_war.json:37` | The enemy AWACS's effect: it cuts our fighter threat by up to 50 % of the share of the day it is on station. |
| tuning `intel.launch_sighting_m` = 20 000 | `data.hpp:830` | How far a surviving pilot reports a SAM launch, after the mission, as intel (§1.6). |

`TheatreDef::year` = 1985 (`data.hpp:176`; `data/air_war/theatre_pannonia.json:12`) gates the types through
`AircraftType::in_service_year` / `InService(year)` (`data.hpp:357, 367`). Only the in-service check and `arm_em_by_year`
(the F-4G fired the AGM-78 until HARM in 1987, `data.hpp:343-346, 369-375`) tie a type to the war's year.

### 1.3 The planner: escort sizing and the expected fighter losses (`planner.hpp`)

- `PlanContext::awacs` (`planner.hpp:54-56`) is the share of the day the AWACS orbit is up.
- `PlanContext::FriendlyFighters(front)` (`planner.hpp:62-65`):
  `f.friendly_fighters * (1 - awacs_fighter_cut * awacs)`.
- Escort building (`planner.hpp:733-753`): if `t.escort_ratio > 0` and `FriendlyFighters > 0.05`, then
  `want = round(Main() * escort_ratio * FriendlyFighters * 2)`, capped at 4. It takes the squadron from
  `PickSquadron(FlightRole::Escort, n, Profile::Medium, ...)`. The escort flight gets the **same `MainRoute(entry, exit,
  target, ...)` as the strike, at medium height**. It has no separate sweep route, no CAP station, no push ahead of the
  package and no rendezvous logic.
- Expected losses (`planner.hpp:1029-1032`): `FriendlyFighters * escort_fighter_kill * (Has(Escort) ? 0.3 : 1.0) *
  Main()`. These are added to total and main losses. (Unlike the resolver, the planner has no night factor.)
- Because the AWACS cut is applied inside `FriendlyFighters`, an AWACS on station also **reduces the number of escorts
  the planner asks for**.
- Package templates with escorts (from `data/air_war/faction_*.json`):
  - NATO: `strike` 0.5, `dead` 0.25, `armed_recce` 0.25 (`faction_nato.json:39-43`).
  - Warsaw Pact: `strike` 0.5, `dead` 0.25 (`faction_warsaw_pact.json:35-38`).
  - Neutral: `strike` 0.25 (`faction_neutral.json:26`).
  - Rebels: none.
- The front's threat values: in `theatre_pannonia.json` (lines 23, 36, 49, 62) rebels 0.15, neutral 0.25, Pact 0.35
  and NATO 0.4; in `theatre_pannonia_nation.json:12-13` Pact 0.35 and NATO 0.4.

### 1.4 The resolver: the only "air-to-air" anywhere (`resolve.hpp`)

- `ResolveContext::awacs` (`resolve.hpp:46`).
- In `Resolver::Resolve`, per flight and per route leg (`resolve.hpp:134-160`, the loop over `order`, then over `si`),
  after the ground defences (`resolve.hpp:231-247`):

```cpp
// The defender's fighters, where the package has no escort.
if (front != nullptr && main && si == 1 && alive > 0) {
    const double ff = front->friendly_fighters * (1.0 - tune.Get("support.awacs_fighter_cut") * Clamp01(c.awacs));
    const double pk = ff * tune.Get("attrition.escort_fighter_kill") *
                      (p.Has(FlightRole::Escort) ? 0.3 : 1.0) * (p.part == DayPart::Night ? 0.5 : 1.0);
    for (int n = 0; n < alive; ++n) if (c.rng->Chance(pk)) { /* LossRecord, lr.by left "" */ }
}
```

  - Only the `main` flights are exposed: `Strike`, `Dead` and `Helicopter` (`resolve.hpp:139`). SEAD, hunters, jammers,
    recce, decoys, escorts, support orbits and cruise missiles are never touched by our fighters.
  - The chance is applied once per flight, on its second route leg (`si == 1`). It does not depend on where the leg
    is, on our bases, on the radar picture or on the GCI.
  - The comment says "where the package has no escort", but the code still applies it with an escort, at 0.3×.
  - The escorts are never lost, never engage, never jettison and never fire. Our fighters are never lost.
  - The loss is written with `LossRecord::by = ""`. `state.hpp:130` says `""` means "fighters or unknown". In
    `campaign/outcomes.hpp:127` such a loss counts as `downed_by_others`. The enemy's intel skips it for launch
    sightings (`campaign.hpp:1067-1068`).
  - The leader's nerve (`resolve.hpp:115-121, 249-255`) can turn the package back after losses, fighter losses
    included. This is the only "adjustment in the fight".
- Docs: `docs/air_war/05_EXECUTION.md:40` ("the defender's fighters take their chance on unescorted packages"),
  `04_PLANNING.md:55-56, 74`, `13_PACKAGES.md:29-30`, `11_SUPPORT.md:22-27`.

### 1.5 AWACS, tankers and ELINT: orbits by the day, not aircraft in the fight

- `AirCampaign::SupportNow` sets `support_.awacs` from the sorties flown (`campaign.hpp:605, 622-646`). It is passed to
  the planner and resolver as `rc.awacs` / `pc.awacs` (`campaign.hpp:605, 799`).
- AWACS types: NATO `e3a` (`faction_nato.json:158`) and Pact `a50` (`faction_warsaw_pact.json:130`,
  `in_service_year` 1985, "very few in a war of 1985").
- An AWACS has no radar, no picture and no vectoring. It is the 0.5 cut only (`docs/air_war/11_SUPPORT.md`).

### 1.6 What the enemy learns in flight: after the fact only

- `campaign.hpp:1056-1065`: launches seen by survivors become `IntelSource::LaunchSighting` reports, queued with the
  pilots' report delay (`pirep_h`). Claims of radars silenced become `PilotReport` (`campaign.hpp:1045-1054`). These go
  into the next planning cycle's picture, never into the running package.
- There is no in-flight radio, no report to HQ, no retasking and no on-call vectoring. `on_call_fraction`
  (`data.hpp:432`) only holds sorties back from the plan (`planner.hpp:297`).

### 1.7 A raid played as the player's scenario (`raid_scenario.hpp`)

- Flights become scenario targets with fixed waypoints. Ordering (`raid_scenario.hpp:407-421`): main flights, then
  SEAD, decoys, recce and jammers. **Escorts rank last (default 5)** and are the first cut at `scenario.max_targets`.
  What is cut is resolved abstractly.
- Every enemy aircraft gets `board_type` 9 (no IFF), or 0 for a jammer (`raid_scenario.hpp:483`).
- `evade` (`raid_scenario.hpp:499-511`): a helicopter dives on lock. A jet with an RWR evades on lock, without one on
  launch, with `break` (`jink` when low). The trigger is only the Osa's own lock or launch.
- The aircraft carry `rwr`, `arms` (ARMs) and jammers. They carry no radar emitter, no AAMs and no air-to-air
  behaviour. The scenario target schema (`scenario.hpp:1281-1284`) has no key for an airborne radar or AAM.
- `package_injector.hpp` puts the same targets into a running session (`AddTargets`). They follow their waypoints too.

### 1.8 The dynamic-campaign testbed (`test_waves.hpp`, `test_waves_data.hpp`, `data/gameplay/test_waves.json`)

- The header (`test_waves.hpp:1-11`): "escorts sweeping ... Ours fly combat air patrols over the city, intercepts out
  toward a front, ground attack out to the front, transports and trainers across, helicopters low."
- Enemy `Escort` (`test_waves.hpp:428-433`) flies entry, then a point within ±8 km of the centre, then 45 km beyond,
  then home, at medium height. It has evasion on lock or launch like the others (`503-509`) and `board_type` 9 (`511`).
- **Our flights**: `FriendlyFlight` (`test_waves.hpp:557-603`) draws from `WaveFriendlyType` (`test_waves_data.hpp:33-45`:
  id, name, model, class, roles `cap|intercept|ground_attack|transport|helicopter|trainer`, speed, height, rcs, count).
  - `cap`: an orbit 8 km toward the enemy's front, 4 laps.
  - `intercept`: from 25 km out, over the centre, 55 km toward the front, then back.
  - Each aircraft is set `t.friendly = true` and `t.board_type = 1` (`592-593`), with no evade and no RWR.
- Our types (`data/gameplay/test_waves.json:47-58`, call signs Sokol, Berkut, Strizh, Kobra, Grom):
  - MiG-21 and MiG-23ML (cap, intercept);
  - Su-22M3 (ground attack); L-39 (trainer, ground attack);
  - An-26 and An-2 (transport);
  - Mi-24D, Mi-8 and Mi-2 (helicopters).
  - All are game placeholders: "our side has no faction file".
- **No interaction**: our CAP or interceptors never engage the enemy, and the enemy's escorts never engage ours. The
  neighbours' fire skips our aircraft by the truth (`scenario_runner.hpp:2282`, `if (t.friendly || ...) continue;`).
- **Era slip**: the deck of enemy types (`test_waves.hpp:353-357`) ignores `InService(year)`. The testbed flies every
  type "whatever the war's year" (`test_waves.json` notes), including TLAM-C (1986) and AGM-86C (1988).
- `docs/DYNAMIC_CAMPAIGN_TEST.md:54-55, 134-135`: "Our side's aircraft are placeholders".

---

## 2. The aircraft types: what they carry today (`data/air_war/faction_*.json`)

Every type has: roles, class, rcs, cruise and attack speed, low and medium height, `max_turn_deg_s`, night and all
weather, `rwr_em`, the jammers, the ARMs, `strike_power`, `loads` (ground weapons only), `computing`, `radius_km`,
`recce_quality`, `air_to_air` and `evasion`.

| Faction | Type | Roles | air_to_air | turn °/s | RWR | Line |
|---|---|---|---|---|---|---|
| NATO | F-15C | escort | 0.95 | 14 | nato_alr46_class_rwr | `faction_nato.json:127` |
| NATO | F-16A | strike, dead, escort | 0.75 | 14 | nato_alr46_class_rwr | `faction_nato.json:108` |
| NATO | F-4E | strike, dead, sead | 0.4 | 9 | ALR-46 class | |
| NATO | F-4G | sead, hunter | 0.2 | 9 | ALR-46 class | |
| Pact | MiG-23MLD | escort, decoy | 0.7 | 12 | generic_rwr | `faction_warsaw_pact.json:86` |
| Neutral | Draken-class (gun-armed) | escort | 0.5 | 12 | none | `faction_neutral.json:74` |
| Rebels | none | | | | | |

- **No radar, AAM, gun, g limit or IR field exists.** No weapons load is an AAM. The F-15C and MiG-23MLD have
  `loads: []`. The Draken's "guns only" is a note, not data.
- The numbers are placeholders: "a period NATO type named for flavour; no value is the real aircraft's" for the F-16A
  and F-15C, "numbers placeholders" for the MiG-23MLD.

### 2.1 Weapons and munitions: no air-to-air missiles

- `data/air_war/weapons.json`: 37 weapons, of the kinds `atgm`, `bomb`, `cluster`, `guided_bomb`, `gun`, `missile`
  (air-to-ground), `retarded_bomb`, `rockets` and `runway`. The guns `gau8`, `gsh30` and `door_guns` are strafing
  loads. **No AAM**: no AIM-9, AIM-7, R-60, R-23/24, R-13, R-3S, Matra or Rb.
- `data/scenery/munitions.json`: 62 munitions (bombs, rockets, ATGMs, ASMs, ARMs, cruise missiles, our SAMs and the
  AAA shells). **No AAM.** (The Sparrow at line 278 is the AGM-78's airframe in an ARM's citation.)

### 2.2 Airborne radars in the EM layer: emitters only, never on an aircraft in play

- `data/em/nato_1985.json:21`: `nato_fighter_fcr_x`, "NATO fighter fire-control radar, X band (APG-63/66 class)",
  `kind: tracking_radar`, a sector scan of 120°. Mostly estimates.
- `data/em/generic.json:54`: `generic_airborne_fcr_x`, a placeholder.
- Used only by the EM demo, the EM bench and tests (`em/demo_scenario.hpp:73, 114`; `examples/em_bench.cpp:69`;
  `tests/test_em_data.cpp:145`; `tests/test_em_scenarios.cpp:157, 626, 686`; `tests/test_em_replication.cpp:69`). It is
  there to be heard by ESM, not to detect, track or guide anything.
- RWRs: `nato_alr46_class_rwr`, `generic_rwr` (ESM entries of kind `rwr`). They hear our radars' lock and launch
  (target_behaviour evasion, ARMs).
- `em::Platform` (`em/model.hpp:70-82`) has `Side side` and `bool iff_reply` ("codes and modes not modelled yet",
  line 79). The EM layer has `EmitterKind::Iff` (`em/emitter.hpp:27`) but no data entry uses it.

---

## 3. Our side's aircraft

### 3.1 Order of battle and chain (data only, not flown)

- `data/command/oob_pannonia.json`:
  - `:11`: the National Air Defence "Holds the fighters ...".
  - `:189-203`: `fighter_division`, with `fighter_rgt_1` (MiG-21 ×36, Kecel), `fighter_rgt_2` (MiG-21 ×36, Bory) and
    `fighter_rgt_3` (MiG-21 ×24, MiG-23ML ×12, Tiszavar).
  - `:349-365`: the Air Force, with `fb_regiment` (Su-22M3 ×26), `recce_squadron` (Su-22 ×8), `training_regiment`
    (L-39 ×30, "an armed squadron of about 12") and the helicopter regiment.
- `data/command/chain_pannonia.json:14-21`: the sector command post's duty officer "knows where our own fighters are".
  `fighter_rgt_duty` (Capt Szilagyi, Kecel; means: in person, field telephone, radio voice) is a post of the chain.
  **Nothing sends or receives a fact through it**: it is referenced only in `chain_pannonia.json` and
  `oob_pannonia.json:191`.
- `data/command/places_pannonia.json:71`: `kecel_airbase`.
- Theatre objects `kecel_airbase` (value 0.9) and `tiszavar_airbase` (0.8), `ObjectKind::Airfield` (`types.hpp:53`), are
  strike targets. **Their damage does not change `friendly_fighters`**. Nothing links them, although
  `docs/campaign/02_WHAT_WE_DEFEND.md:19` says "Fewer of our fighters up: the enemy brings fewer escorts".
- `docs/campaign/10_BRIEFING_FORCES_AND_NATION.md`:
  - `:124-128`: the fighters sit with the National Air Defence; the sector directs the interceptions.
  - `:160-169`: the regiments, "Interception under the sectors' control; escort". The MiG-23MLs are "the best fighters
    the country has". The Pact also flies MiG-23s: "one more reason a battery must know which of ours are up and trust
    its IFF (roadmap 17.2)".
  - `:278`: **"No MiG-29s in 1980-85"** (decision 2).

### 3.2 Basic models (presentation)

`data/scenery/basic_models.json:127-135`: `mig21` "(our fighter regiment)", `mig23ml` "(our 3rd Fighter Regiment)",
`su22m3`, `l39`, `an26`, `an2`, `mi24d`, `mi8` and `mi2`, in `own_*` colours. The enemy's `f15c` (`:98`) and `mig23mld`
(`:105`) also exist.

### 3.3 Friendly aircraft in scenarios (authored, waypoint-following)

`"iff": "friend"` (`scenario.hpp:96, 758-764`, `ScenarioTarget::friendly`) appears in:
- `data/scenarios/kesmark_range_test.json:344` ("Our own fighters: they answer the interrogator"), formation 2;
- `data/scenarios/tutorial_basics.json:157` (our fighters to look at);
- `link_osa_21.json:217`, `pu12m_battery.json:82` and `pu12m_range_day.json:274` ("Own transport (placeholder)").

The testbed's waves add more (§1.8). None of them fights.

---

## 4. IFF for the battery today

### 4.1 The Osa-AK console (`data/procedures/osa_ak.json`, `engagement.hpp`)

- The `search.iff` block (`osa_ak.json:242-246`): `query` ЗАПРОС, `control_query` ЗАПРОС К, the gate control КЛАПАН,
  `ready` soc_transmitter (it works only with the search radar radiating), `query_time_s` 1.0 and `max_range_m`
  30 000. Sources: mon_2112_80 p21, voenizdat_osa_1991 p17/p53/p55/p56. "Range and query time are the 1991 textbook's
  for the AKM's 1L24-2; the Osa-AK's interrogator may differ."
- The controls `iff.query`, `iff.query_k` and the gate are the senior operator's (`osa_ak.json:475-480`). The input
  `iff_interrogating` is at `:324`. The procedure `iff_interrogate` is at `:1526-1531`.
- The loader is `SearchSensorDef::iff_*` (`console_data.hpp:44-53, 1712-1727`).
- `EngagementModel::UpdateIff` (`engagement.hpp:2273-2340`):
  - A press opens a 1 s interrogation. With the EM layer, replies ride on the search radar's own tracks from the
    answering transponder (`em_link.hpp:438-441, 576`; `em/radar_system.hpp:398-400`: `t.iff_reply && t.side ==
    p.side`, per look with `iff_reply_probability` 0.9).
  - Without the EM layer, every `friendly` target with line of sight in range replies, or only those on the scope if
    gated.
  - Contacts asked and silent become `IffNoReply` ("Ответа нет!"), a reply becomes `IffFriend`, and both are logged.
- `KnownFriend` / `NoReply` (`engagement.hpp:1059-1062`) are the crew's memory of a reply.
- Firing at a friend is allowed but logged as a deviation: `"fired at a friendly aircraft"` (`engagement.hpp:915`). The
  shot then resolves like any other, so **friendly fire is physically possible for the player**.
- NPC behaviour (`scenario_runner.hpp`):
  - The senior operator interrogates each new echo in reach (`10397-10408`) and reports `iff_friend` / `iff_no_reply`
    (`10446-10453`, `ReportNoReply` `10016-10052`).
  - The NPC commander never launches at a `KnownFriend` or a friend he was told of (`pictures_[Cmdr()]`). He orders
    `hold_fire` and `order_drop_target` (`8767-8775`), and picks only non-friends (`8688`).
  - Crew knowledge: `crew_knowledge.hpp:28-29` (`friendly`, `interrogated`).
- Tests:
  - `test_engagement_model.cpp:454, 486`;
  - `test_crew_systems.cpp:78, 103`;
  - `test_em_link.cpp:761`;
  - `test_crew_training.cpp:214`;
  - `test_battery_link.cpp:77`;
  - `test_command_post.cpp:768`;
  - `test_pu12m_battery.cpp:264`.

### 4.2 The PU-12M and the regiment's picture

- A cabled radar's IFF (`command_post.hpp:11-15`): "A target is a friend on it only if that radar's interrogator works
  and its transponder answers". `scenario_runner.hpp:6293-6301`: `e.type = def_.post_radar.iff_interrogator &&
  c.transponder ? 1 : 9`. `RegimentRadar::iff_interrogator` (`c2_sources.hpp:102-104, 259-263`;
  `data/command/regiment_radars.json:14`: P-19 with the 1L23-6 for Kremniy-2/2M and Parol-4, p19_to 4.3.3; `:23`:
  P-40, an estimate).
- **The regiment's telecode and voice picture takes identity from the scenario's truth**: `BoardContact::type =
  t.board_type >= 0 ? t.board_type : (t.friendly ? 1 : 9)` (`scenario_runner.hpp:6600, 6622`; `pu12.hpp:76, 93-95`). It
  has no regiment-level interrogation, no notice from the fighter regiment and no error.
- The ДХ keypad's СВ (friend) and foe keys (`data/procedures/pu12m.json:230-246`) let the battery commander mark a track
  by hand: `CommandPost::SetAttributes` / `ApplyOverrides` (`command_post.hpp:469-485, 963-970`;
  `scenario_runner.hpp:6431-6432`).
- The codes: `data/command/aspd_codes.json:14` has code 40 "own aircraft in the zone" ("Shown only: check every target's
  IFF before firing"). `talk_tables.json:19` has group 402 «В зоне свои самолёты». **No scenario, tutorial or code
  sends either.**

### 4.3 Defences that never err on IFF (truth-based)

- The battery fire model (the regiment's other Osa launchers): `BatteryFireTarget::friendly` from the truth
  (`scenario_runner.hpp:10678, 10753`), skipped in `TakeNearest` (`battery_fire.hpp:57, 337`).
- The testbed's neighbours skip `t.friendly` (`scenario_runner.hpp:2282`).
- The abstract air war has no friendly aircraft, so no fratricide.
- The debrief's judge of the commander's choice reads the truth (`scenario_runner.hpp:8153-8194`; `crew_decisions.hpp:143`
  gives a friend's score as −1).

### 4.4 What IFF lacks (relevant to 17.2 and to air-to-air)

- No codes, modes or code changes, and no Mode 4-like crypto. No Soviet system identity (Kremniy-2 vs Parol) beyond
  citations. No transponder failures, and no enemy spoofing or a captured transponder.
- No notice of our flights (corridor, times, heights) through the pipeline.
- The other ID means are all absent: visual ID by the TV (`em/optical.hpp:28, 40, 94` recognises only "a drone told
  from an aircraft"), behaviour (in the corridor), and the regiment's word.

---

## 5. Hot-phase flight today (what any air combat would sit on)

- `aircraft.hpp:1-30`: "constant airspeed, turn-rate-limited steering toward the next waypoint". No energy, no g, no
  bank. The prototype mirrors it (`prototype/samproto/engagement.py:18`, class `Aircraft`).
- `target_behaviour.hpp:1-15, 33-59`:
  - `TerrainFollowing` and `PopUp`;
  - `EvadeDef` with `On {Launch, Lock, Order}` and `Kind {Break, Dive, Jink}`.
- Damage: `damage.hpp:5, 74-76, 107, 265-271`. Moderate or heavy damage may jettison, abort or crash
  (`scenario_runner.hpp:2183, 2636`). `TestWaves::Jettison` is at `test_waves.hpp:164-177`.
- Track R:
  - R3 (bank-to-turn within each type's g and roll-rate limits, "the type limits are cited in the air war's aircraft
    data") is **not done** (`docs/DEVELOPMENT_ROADMAP.md:298`).
  - R4 (reactions to any radar's RWR warning, to launches or flak seen, and to damage; jettison on "a heavy threat,
    **fighters**, damage") is **not done** (`:299`). Its "What is there today" list (`:275-287`) still holds for
    evasion.

---

## 6. What the roadmap promises and rules out

| Item | Where | Text (abridged) |
|---|---|---|
| A14-A15 | `docs/DEVELOPMENT_ROADMAP.md:180` | "Planned in the battery's war ... our fighters in the battery's sky and in the enemy's escort need (17.2) ... **Our air force as a planner is not planned**" |
| A15 | `docs/air_war/08_IMPLEMENTATION_PLAN.md:20` | "Our side's air force as a player of the same game (**the defender's fighters are a number today**) — In part: our fighters in the battery's sky, IFF and the enemy's escort need (17.2). Our air force as a planner is not planned" |
| 17.2 | `DEVELOPMENT_ROADMAP.md:443`; `docs/campaign/09_IMPLEMENTATION_PLAN.md:101` | "**Our fighters**: the friendly corridor in the order, IFF codes and their changes, the fighter regiment's notices through the pipeline, friendly aircraft in the scenario, friendly fire when a notice or the codes did not arrive". Status: **not started** (phase 17's header, `:435`, lists 17.1, 17.3, 17.4 in first form only) |
| Phase 17 done-when | `DEVELOPMENT_ROADMAP.md:448-450`; `09_IMPLEMENTATION_PLAN.md:106-107` | "a MiG-21 in the corridor answers IFF on the player's scope, and a battery left without the notice fires on it" |
| 17.2 design | `docs/campaign/05_THE_SYSTEMS.md:230-232` | "friendly aircraft in the battery's sky by the task's coordination (the corridor and its times), written as targets that answer IFF. The fighter regiment's notices reach the battery through the pipeline (its duty officer is already a post), and friendly fire follows from stale codes or a notice that never arrived" |
| 14.1 coordination | `DEVELOPMENT_ROADMAP.md:380`; `03_THE_BATTERY_MISSION.md:58, 84`; `05_THE_SYSTEMS.md:96` | The task's coordination: "the friendly air corridor and its times, IFF codes, call signs". Built only as free text: `BatteryTask::coordination` (`campaign/battery_task.hpp:109, 175, 324`); `data/campaign/tasks/bty21_cover_bridges.json:41` "our fighters' corridor north of Kesmark" |
| Air bases | `docs/campaign/02_WHAT_WE_DEFEND.md:19` | The air bases' role: "Our fighters, which the enemy must escort against (`friendly_fighters`)". The link from the bases' damage is not built |
| R4 | `DEVELOPMENT_ROADMAP.md:299` | Pilots react to warnings, sighted launches or flak and damage; jettison on "a heavy threat, fighters, damage" |
| Not planned | `DEVELOPMENT_ROADMAP.md:542-544`; `:603` ("Later"); `09_IMPLEMENTATION_PLAN.md:202` | "Not planned: the regiment's posts for the player, the neighbours at full detail, **our air force as a planner**, a ground war simulation, an economy, more than one enemy at a time" |
| Air superiority | `docs/campaign/08_COMMAND_DOCTRINE_AND_REACH.md:19-21` | Air superiority is a state the campaign moves through, reached **when the enemy believes our air defences are broken** (the SAM rollback). Fighter-vs-fighter plays no part in it |
| Research | `docs/research/ENEMY_AIR_RESEARCH_2026-10.md:113-124, 150-162` | The Pact's tactical groups (clearing the airspace, escort), strike groups with escort through suppressed corridors (`cyberleninka_airfields`), NATO packages with "sweeps and escorts". No sources on air combat tactics, AAMs or GCI are registered |

**Nothing in the roadmap, the campaign docs or the air war docs promises:**
- enemy-vs-our-fighter engagements, AAMs, airborne radars, BVR or WVR combat;
- in-flight retasking, flight-to-flight or flight-to-HQ radio, or GCI and AWACS vectoring;
- our fighters as simulated flights in the campaign.

R4 touches pilots' reactions (including to "fighters"), but as a defensive reaction only.

---

## 7. Cold War consistency (the owner's instruction: Cold War, not modern tactics)

What the repo already pins to the period, and must keep:
- The war's year is 1985 (`TheatreDef::year`). Types fly only when in service (`in_service_year`). The ARM changes by
  year (`arm_em_by_year`). The A-50 is barely in service and the E-3A is in NATO from 1982 (`11_SUPPORT.md:11-14`).
- Decision 2: no MiG-29 for us in 1980-85; the MiG-23ML is our best fighter (`10_BRIEFING_FORCES_AND_NATION.md:166, 278`).
- Our fighters work "under the sectors' control" (`10_BRIEFING_FORCES_AND_NATION.md:124-128, 164`): ground-controlled
  interception from the sector command post. This is the period's Warsaw Pact pattern. The neutral's Draken is
  guns-only, after the Austrian ban (`faction_neutral.json:74`).
- The `data_link` C2 source is flagged "late (PU-12M7 and later; not 1980s Osa-AK)" (`data/command/c2_sources.json:110-115`).
  Digital data links are not the 1985 battery's means.

Where today's code slips from the period:
- The testbed ignores `InService` (§1.8).
- The F-16A is an `escort` with `air_to_air` 0.75 and no AAM load to check against. Period background to source before
  use: USAF Europe's F-16A of 1985 is held to have carried only short-range IR missiles (no Sparrow before the ADF
  conversion), so it should be a visual-range fighter.

Period background for a later design, to be verified and cited under the project's source rules (not taken from the
repo):
- No AMRAAM (in service 1991). NATO's BVR is the semi-active AIM-7F/M on the F-15C and F-4, under rules of engagement
  that often required visual or AWACS identification.
- The Pact's MiG-23ML/MLD carry R-23/R-24 (semi-active or IR) and R-60.
- The MiG-21bis carries R-60, R-13M and R-3S. The R-73 and helmet sight come with the MiG-29/Su-27.
- No Link 16 or JTIDS for 1985 fighters.
- Soviet-pattern interception is tightly ground-controlled: voice, and the Lazur-type command link where fitted.
- Soviet IFF is Kremniy-2 and Parol; NATO's is Mk XII with Mode 4.

---

## 8. Gaps against the request (air-to-air and our aircraft)

1. No air-to-air engagement of any kind: no aircraft-vs-aircraft detection, track, launch, missile flight, gun pass
   or kill. The resolver's flat `escort_fighter_kill` is the whole model.
2. No sensors on aircraft: no airborne radar on any type in play, no IR or visual search, no RWR against airborne
   radars (RWRs hear only our ground radars). `nato_fighter_fcr_x` is an emitter for ESM tests only.
3. No AAMs in `weapons.json` or `munitions.json`. No air-to-air guns as data (the guns are strafing loads).
4. No flight model for combat: no energy, g, bank or roll (R3 not done). The prototype mirrors `aircraft.hpp`.
5. Escorts do nothing but scale a chance. They fly the strike's route at medium height, with no sweep, no CAP, no
   engagement, no losses, and they are the first cut from a played raid.
6. `air_to_air` is loaded and never used.
7. Our fighters are not simulated in the air war: no squadrons, sorties, bases, GCI, readiness or losses. Struck air
   bases do not change `friendly_fighters`. The fighter-loss record has `by = ""`.
8. Our flights in the testbed are scenery: CAP and intercept geometry with no targets, and perfect IFF to neighbours.
9. The AWACS has no radar and no vectoring; it is a 0.5 multiplier. There is no enemy GCI, no AWACS-to-fighter
   control, and no our-side GCI from the sector command post.
10. No live comms between flights, HQ or squadrons. Pilots' reports reach the enemy's intel after landing only. No
    retasking in flight.
11. IFF is binary: no codes, modes, notices or corridor. The regiment's picture identity comes from the truth. The other
    batteries and neighbours have perfect IFF. Code 40 and group 402 exist but are never sent.
12. No ID of a friendly MiG-23ML against the Pact's MiG-23MLD beyond the transponder (the TV recognises drone vs
    aircraft only).
13. The enemy's air superiority is defined by SAM rollback alone. The fighter balance plays no part.
14. The testbed flies out-of-period types.

---

## 9. Hooks: where new air-to-air work would attach

- **Roles**: `airwar::FlightRole` (`types.hpp:177`) and `FlightRoleName`. Add `Sweep`/`Cap`, or split `Escort` into
  close escort and sweep, then follow every `switch` on it (raid_scenario rank `:408-420`, test_waves `EnemyFlight`
  `:352`, outcomes ledger `:126`).
- **Type data**: `airwar::AircraftType` (`data.hpp:318-376`) and its loader `LoadAircraft` (`data.hpp:1335-1400`). Add a radar (EM id),
  AAM loads, a gun, g and roll limits (R3 shares these), and IFF. `air_to_air` is the existing hook for a quality
  factor.
- **Weapons**: `WeaponsDef` / `WeaponDef` (`data.hpp:303-313`, `data/air_war/weapons.json`) and munition shapes
  (`data/scenery/munitions.json`, `scenery/munitions.hpp`) for the AAMs.
- **Abstract resolution**: `Resolver::Resolve` (`resolve.hpp:231-247`) replaces the flat fighter chance with an
  air-battle exchange (our sorties up from `friendly_fighters`, escorts' `air_to_air`, AWACS and GCI, losses on both
  sides). `LossRecord::by` (`state.hpp:130`) could name "fighters".
- **Planner estimate**: `PlanContext::FriendlyFighters` (`planner.hpp:62-65`), escort sizing (`:733-753`), expected
  losses (`:1029-1032`). Keep planner and resolver one function, as `PassKillChance` is.
- **Our fighters' strength**: `FrontDef::friendly_fighters` (`data.hpp:158`) could become derived from `oob_pannonia.json`
  fighter regiments and the function of `kecel_airbase` / `tiszavar_airbase` (`TheatreObject`, `ObjectState`).
- **Played sessions**: `RaidScenarioBuilder::Build` (`raid_scenario.hpp:407-560`) and `PackageInjector` write targets;
  the `ScenarioTarget` schema (`scenario.hpp:82-125, 1281-1320`) and `TargetPilot` (`target_behaviour.hpp`) are where
  airborne sensors and AAM behaviour would live. The runner already flies weapons as session objects: ARMs
  (`ArmCarrierDef`, `scenario.hpp:66-80`, struct at `:71`; `em/arm.hpp`), and cruise missiles as `weapon` targets.
- **Testbed**: `TestWaves::FriendlyFlight` and `EnemyFlight` (`test_waves.hpp:352, 557`), `WaveFriendlyType`
  (`test_waves_data.hpp:33`): the first place our CAP and the enemy's escorts could meet.
- **EM**: `em::Platform::side` and `iff_reply` (`em/model.hpp:70-82`), `EmitterKind::Iff`, and `nato_fighter_fcr_x`
  (`data/em/nato_1985.json:21`) as an airborne radar heard by our ESM and the RWRs.
- **17.2 (IFF and corridor)**:
  - `ScenarioTarget::friendly` and `board_type`;
  - `BoardContact::type` (`scenario_runner.hpp:6600, 6622`), to come from a notice instead of the truth;
  - `CommandPost::SetAttributes`;
  - `aspd_codes.json` code 40 and `talk_tables.json` group 402;
  - `BatteryTask::coordination` (free text today);
  - the pipeline post `fighter_rgt_duty` (`chain_pannonia.json:18`);
  - truth-based skips to revisit for fratricide: `battery_fire.hpp:337`, `scenario_runner.hpp:2282`.
- **Reactions (R4)**: `EvadeDef` (`target_behaviour.hpp:49-59`) and `damage.hpp`'s jettison; a "fighters" trigger
  belongs here.

---

## 10. Constraints new work must respect

- simcore must be header-only C++17 with the standard library only. No exceptions or RTTI. Avoid the macro names
  (`check`, `verify`, `min`, `max`, `near`, `far`, ...; `tests/header_check.cpp`). Use SI units with suffixes and the
  ENU frame. Warnings are errors.
- Every new model or behaviour needs a test; physics tests compare against hand calculations or tables. The prototype
  must stay in sync for `aircraft.hpp` changes (`prototype/samproto/engagement.py`).
- Data needs `source` and `confidence` for every value (`python tools/check_citations.py`; `data/air_war` values may be
  "placeholder" game values only when labelled). Prefer operator-country sources. Public sources only.
- Period: the war of 1985 (`TheatreDef::year`, `in_service_year`). No MiG-29 for us in 1980-85, no late data links, no
  post-Cold War weapons or tactics.
- First person: the player's character learns only through people and means of signal (`simcore/command/`). There
  must be no omniscient HUD of the air battle. Air-to-air results reach the battery only by the nets, the board or
  sight. The written record is the war's memory: reports are built from what crews wrote, not the truth.
- No NPC acts on the truth: IFF and friend status must reach crews through instruments or voice. Every NPC act at a seat
  goes through `ScenarioRunner::NpcDoes`. Run `helper_coverage` after crew changes.
- The air war's planner may know which objects exist but never their positions (`PlanContext::truth`,
  `planner.hpp:53`). One enemy at a time. The strategy layer stays thin. "Our air force as a planner" is out of scope
  per the roadmap.
- The balance must hold: `air_war_balance`, `air_war_bench` (which zeroes `friendly_fighters` for its analogues,
  `examples/air_war_bench.cpp:180`), and the tests that zero it (`test_logistics.cpp:237`, `test_weapons.cpp:44`) or
  read it (`test_country_map.cpp:698`). Save and load replays exactly by seed (`air_war/save.hpp`), so new randomness
  must come from the campaign's RNG.
- Unreal: game logic in simcore only; coordinates through `USimWorldSubsystem`; fixed-step simulation.
