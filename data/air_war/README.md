# data/air_war — the enemy air command

Loaded by `simcore/air_war/data.hpp` (`LoadAirWarData(data_dir, theatre_file, faction_file, ...)`), which also loads
`data/air_war/traits.json` and `data/gameplay/air_war.json` and cross-checks them. Design: `docs/air_war/`; research:
`docs/research/ENEMY_AIR_RESEARCH_2026-10.md`. The loaders are strict: unknown keys fail, numbers are range-checked,
every block names `source` and `confidence` (`tools/check_citations.py` checks the sources against `docs/SOURCES.md`).

| File | What |
|---|---|
| `theatre_pannonia.json` | The fictional country, its four fronts (entry points, the enemy's bases, the defender's fighter threat, where the player's battery deploys in a war on that front) and its objects (value, hardness, repair, mobility, emission, pre-war knowledge, air defence, C2 role). Positions in km east and north of `origin` |
| `theatre_pannonia_nation.json` | **The whole nation** (the nation on the map, N2; `docs/COUNTRY_MAP.md`): `"extends": "theatre_pannonia.json"` and adds the briefing's forces, industry and infrastructure on open ground of the country's map, 170 objects, every mobile unit with its prepared positions, and where battery 21 deploys in the wars with NATO and the Pact. Our order of battle (`data/command/oob_pannonia.json`) owns every military object. `air_war_demo` and `air_war_balance` run on it with `--theatre theatre_pannonia_nation.json` (`docs/air_war/09_BALANCE.md` §8). The Pact's and NATO's fronts carry a `force_scale` (2 and 1.5: the rebalance for a whole nation). **The game's new campaigns are fought on it** (N4, 23 Oct 2026); `theatre_pannonia.json` stays the theatre the tests and the first balance pass tuned |
| `faction_rebels.json` | Inferior: the Rava Liberation Front's improvised air wing |
| `faction_neutral.json` | Near peer: the Noric Republic's small air force |
| `faction_warsaw_pact.json` | Peer: the Warsaw Pact's air forces of the front |
| `faction_nato.json` | Superior: a composite allied tactical air force |
| `traits.json` | The eight traits and the 33 quirks with their named effects, signatures and our analysts' phrases (`chatterbox` and `silent_runner` move a unit's radio discipline, B9) |
| `air_combat.json` | **The air battle's air-to-air data** (track B, B7; `docs/air_battle/07_AIR_TO_AIR.md`): `aams` (18 missiles by guidance class with `in_service_year` and `allies_year`, ranges, limits, reliability, design kill chance, lethal radius, and `game_values` naming what is not sourced), `guns`, `fits` (what each type carries by year, `loads_by_year`; ours, the Pact's, NATO's, the neutral's and the rebels'), `doctrine` (rules of engagement, commit, formation, shot order and the strikers' answer per faction and ours; since B8 also `skill`, `on_control_lost` for green and skilled pilots, `commit_odds` and `fighter_plan`: the shares of close escort, sweep and CAP and the sweep's lead), `calibration` (the chain's numbers, the 1967 engagement mix and the period's rates they are held to), `fight` (B8's game values for the live fight, `air_war/air_fight_session.hpp`, and the abstract one, `air_war/air_fight.hpp`: think and scan rates, the eye's identification range, GCI vectors and their loss, joker and bingo, and the A0 chances of finding, meeting and surprise) and `ours` (our fighter regiments' types and counts from the order of battle, which the abstract fight draws our group from; a loss to them is written with the regiment's id). The `unarmed` fit names the support, jamming, reconnaissance and light types that carry nothing. Loaded by `air_war/air_combat_data.hpp` into `AirWarData::air_combat`; the shot is `air_war/air_combat.hpp`. Nothing later than the war's year is offered (`AirCombatDef::LoadOutFor`) |
| `sensors.json` | The airborne sensors by type and year (`03_PERCEPTION.md` section 4): fighter radars (each naming its EM `RadarDef` in `data/em/fighter_radars_1985.json`, with the published ranges), infrared search sets, optical aids, warning receivers by type, flares and command links; `slots` are sensors the research has no year for, out of every war until a source fills them. `AirWarData::sensors` |
| `countermeasures.json` | **Chaff, flares and drop tanks by type and year** (track B, B4; `docs/air_battle/04_REACTIONS.md` section 7): each fit names the aircraft's model id, the kind (`chaff`, `flares`, `drop_tanks`), the count and the year it entered service; `automatic` marks a dispenser started by the warning receiver, not used in the 1985 war. The latest fit not after the war's year counts (`ReactionTable::LoadoutOf`). The ALE-40 and ASO-2V loads are placeholders until a source gives them |
| `weapons.json` | The weapons (facts from `wiki_weapons`; footprint and effect by target class are calibrated game values) and every object kind as a target: its class and aim points (`docs/air_war/10_WEAPONS.md`) |

Each faction file also has a `"radio"` block (track B, B9; `docs/air_battle/09_COMMS_AND_INTERCEPT.md` section 3.1): the EM library ids of its aircraft and controllers' sets, the band of its nets, `language`, `callsign_rotation_h` (0: never), the staff's `opsec` 0..1, `have_quick_share` and `secure_voice_share` (0 until a USAFE fielding date is found), `gci_channels_per_post`, and `discipline`, the share of a strike flight's routine calls kept off the air. Its `source` and `confidence` are required. The call lengths and the rest are the `"radio"` group of `data/gameplay/air_war.json`.

Each faction file also has a `"c2"` block (track B, B5; `docs/air_battle/05_C2_AND_RETASKING.md` section 4.9, loaded
into `FactionDef::c2` by `c2_def.hpp`): `posts` (id, `kind` of `tasking`, `support_centre`, `radar_control`,
`airborne_warning`, `fac` or `squadron`; `at` places them at the faction's `bases`, `entries`, along the front's `line`
`back_km` behind it, or one at every vertex of the line with `line_points`; `reach_km` its radio cover, a FAC's
`sight_km`, `from_year` for a post not yet there in the war's year: NATO's E-3A 1982, the Pact's A-50 1989), `authority`
(which post may `designate`, `warn`, `divert_airborne`, `scramble`, `approve`, `redirect_before_ip`, `divert_recovery` or `recall`,
for which `missions`, `where` and with what `needs` or `chance`), `timing` (each hop's relay, decide and encode bands in
seconds), `alert` (the alert pools' shares of the planner's on-call sorties, take-off times, shifts, an airborne pool, a
`phase_scale`), `report_missions`, `hunter_leave` (`any`, `free_hunt_zone` with `free_hunt`, or `none`),
`silence_consent_s`, `max_report_age_h`, `nomination_h` and the Pact's `debrief_s`. The rebels have no posts. The game
values are the `"c2"` group of `data/gameplay/air_war.json`; its `mode` 0 sets every faction's block aside (the
campaign as before B5, for an old save's replay). Its officers come from the pool's ranks `cp_controller`,
`gci_controller` and `fac`.

## Faction format (summary; `docs/air_war/01_FACTIONS.md` has it all)

```json
{
  "faction_id": "nato", "name": "...", "short_name": "NATO", "tier": "superior", "front": "nato",
  "source": "...", "confidence": "medium", "em_libraries": ["em/nato_1985.json"], "callsigns": ["Hammer", "..."],
  "doctrine": { "planning_cycle_h": 24, "lead_time_h": 12, "on_call_fraction": 0.1, "centralisation": 0.6, "simultaneous_bias": 0.6,
                "sortie_rate_per_day": 2.0, "serviceability": 0.8, "max_packages_per_cycle": 20, "source": "...", "confidence": "medium",
                "phases": [ { "id": "rollback", "name": "...", "apportion": { "dead": 0.25, "strike": 0.25 }, "until_iads": 0.45 } ],
                "packages": { "strike": { "main_flights": 2, "flight_size": 4, "aircraft": ["f111f"], "sead_per_threat": 1.0, "sead_max": 6,
                                          "escort_ratio": 0.5, "jammers": 1, "escort_jamming": false, "decoys": 0, "chaff": true,
                                          "recce_lead": 0, "profiles": ["low", "medium"], "day_parts": ["night", "dawn", "day", "dusk"] } } },
  "intelligence": { "satellite": true, "satellite_pass_hours": [9.5, 21.0], "elint": true, "elint_reach_km": 250, "...": "..." },
  "logistics": { "replacements_per_day": 2, "arm_stock": 60, "arm_resupply_per_day": 3, "damaged_return_h": 36, "source": "...", "confidence": "estimate" },
  "politics": { "loss_tolerance_per_day": 4, "no_strike_city_radius_km": 4, "max_depth_km": 0, "banned": ["city"], "target_weights": {},
                "events": [ { "id": "...", "name": "...", "text": "...", "day_at_least": 4, "reinforce_squadron": "f16_wing", "reinforce_count": 24 } ] },
  "aircraft": [ { "id": "f4g", "name": "F-4G Wild Weasel", "roles": ["sead", "hunter"], "class": "fighter", "arms": 2, "arm_range_km": 18,
                  "source": "...", "confidence": "low" } ],
  "squadrons": [ { "id": "weasels_g", "name": "...", "aircraft": "f4g", "count": 12, "leader": "nato_ldr_weasel" } ],
  "officer_pool": { "ranks": { "commander": "Major General" }, "first_names": [], "last_names": [], "trait_mean": {}, "skill_mean": {} },
  "officers": [ { "id": "nato_cdr", "name": "...", "role": "commander", "traits": {}, "skills": {}, "quirks": ["sead_first"] } ]
}
```

A theatre may **extend** another in this folder (`"extends"`, `LoadTheatreFile` only): it starts from the base, its fronts
replace the base's of the same faction, `"remove"` drops base objects, and its objects are added, one with a base
object's id replacing it where it stands; it keeps the base's `origin` and `country`. An object's `"positions"` (a
mobile unit with `relocate_h`, never the player's battery) are its prepared positions besides its own: it moves between
them, never to one within 8 km of the enemy's front; without them it moves `relocate_km` in a random direction. A
defence's `"channels"` are its fire channels, the aircraft of a passing flight it engages at once (an S-75's or S-125's
guidance radar one, an Osa battery four; `docs/air_war/05_EXECUTION.md` §2); without them it engages every aircraft in
reach. An object's `"c2_source"` makes it a warning post: a raid takes the nearest working one of each kind. A defence's
`"max_target_mps"` is the fastest target it can engage. The theatre's `"year"` is the war's (1985): an aircraft or
missile type with `"in_service_year"` later than it does not fly. A front's `"missile_launch"` is where its ships or
ground launchers fire cruise missiles from, and its `"force_scale"` (0.25-4, default 1) how many times its faction's
strength fights on it: every squadron's aircraft and the packages its staff plans a cycle (not the cruise missiles'
stocks); give its reason in the front's `"notes"`.

A theatre's **`"supply_lines"`** (`docs/air_war/14_LOGISTICS.md`): each runs `"from"` a depot, junction or port `"to"`
troops, by `"net"` `road` or `rail`, `"via"` objects on the way, with `"columns"` of `"column"` kind `convoy`, `train`
(by rail) or `column` (an armoured march column), `"vehicles"`, `"carries"`, `"speed_kmh"`, `"load_h"`, `"delivery"` (of
the troops' stocks, or of their strength for a march column), `"night_only"`, `"value"`, `"hardness"`, `"replace_h"`,
`"prior_known"`, an optional `"defence"`, and its source. The loader makes each column a theatre object `<line>_<n>` at
its depot (an extending theatre's lines replace the base's of the same id). `"country_map"` names the country's map the
lines follow (`maps/pannonia.json` on the nation's theatre).

Each aircraft type's **`"loads"`** (`[["weapon", count], ...]`), `"designator"` (`laser`), `"computing"` (its sight,
0..1), `"radius_km"` (its published combat radius) and the support roles `awacs`, `tanker`, `elint` are in
`docs/air_war/10_WEAPONS.md` and `11_SUPPORT.md`; a doctrine's `"delivery_risk"`, `"min_effect"` and `"min_value"`, and
the logistics' `"buildup_initial"` and `"buildup_full_by_day"` (a squadron's own `"initial"` and `"full_by_day"`) in
`10_WEAPONS.md`, `13_PACKAGES.md` and `12_FORCES.md`.

**Cruise missiles** are entries of a faction's `aircraft` with `"missile": true`, the role `"missile"` alone, a
`"range_km"`, a `"launch"` (`air`: its carrier releases it at the border entry; `sea`: from the front's
`missile_launch`), a `"hit_chance"`, a `"stock"` and `"resupply_per_day"`, and the class `cruise_missile`; the doctrine's
`"missile"` package template (`flight_size` missiles of the first type in service with a salvo in stock, its profile:
`high` for a dive from height, `low` for terrain following) and a share in the phases. The Pact has the Kh-22M (1970s);
NATO the BGM-109C Tomahawk (1986) and AGM-86C CALCM (1988).

A faction's **`"flights"`** block (track B, B1; `docs/air_battle/02_FLIGHTS_AND_PILOTS.md` sections 1.3 and 2) is its
formations: per class (`fighter`, `strike`, `helicopter`) and phase (`transit`, `ingress`, `attack`, `egress`,
`defensive`, `orbit`), `"slots"` as `[right_m, back_m, up_m]` of the leader in number order (the leader's `[0, 0, 0]`
first, then No. 2, the element lead No. 3, his wingman No. 4; a place past the list repeats it further back), each phase
with its `source` and `confidence`, and the block's own. A phase not given flies the transit's; a class is picked from
the flight's role (`FormationClassOf`: escort, SEAD, hunter, jammer and the support roles fly as fighters, helicopters as
helicopters, the rest as strike). Strict keys; 1985's formations only. `RaidScenarioBuilder` and the testbed's waves
write a raid's flights with them (the scenario's `"flights"` and `"packages"`, `scenario.hpp`); the runner keeps the
flights and each wingman flies his slot on his leader (`simcore/flights.hpp`).

```json
"flights": { "source": "...", "confidence": "low", "notes": "...",
  "fighter": { "transit": { "slots": [ [0, 0, 0], [1600, 0, 600], [-3200, 0, 0], [-4800, 0, 600] ], "source": "anderegg_2001 ...", "confidence": "medium" } },
  "strike": { "transit": { "slots": [...], "source": "...", "confidence": "low" }, "ingress": {...}, "attack": {...} },
  "helicopter": { "transit": {...} } }
```

**Routes as flown** (track B, B2; `docs/air_battle/01_ROUTES.md`). An aircraft type's **`"low_level"`** says how low and
how fast it flies over enemy ground: `"agl_m"` by day, `"night_agl_m"` on terrain-following radar (only a type that has
one), `"mps"` (default its `attack_mps`). Each block has its own `source` and `confidence`. NATO's come from
`rafhs_j20_1999`, `afm_51_40_1973`, `agard_cp267_1979`, `crandall_twz` and `napier_hushkit_2022`. The Pact's heights
come from `fm_100_2_1_1984` and `opp_85`; its speeds are game values. A doctrine's **`"routing"`** block is the
faction's route rules: `"crossing"` (`gate` NATO, `checkpoint_gate` the Pact, `entry` the neutral, `none` the rebels),
`"cas_free"`, `"gate_window_s"`, `"gate_separation_s"`, `"letdown_m"`, `"max_turn_points"`, `"variants"` (`covered`
first, then `other_flank` and `direct`), the leg cost's weights `"w_exp"`, `"w_cont"`, `"w_low"` and `"lines_bonus"`,
`"return"` (`other_gate`, `planned_departure`, `same_entry`), the group ahead's `"blast_lead_s"` and
`"blast_exposure_s"`, `"zone_escapes"` (the Pact's counter-SAM programme), `"helo_umbrella"`, `"corridor_suppression"`,
and who may leave the route in flight: `"deviation"` (`leader`, `ground_control`, `eye`, `any`) with
`"deviation_lateral_m"` and `"deviation_tot_slip_s"` (for B4 and B5). Strict keys. The route planner
(`simcore/air_war/route_planner.hpp`) reads both when the tuning's `route.mode` is 1. A save made before B2 loads with
mode 0 and flies the old shapes.

Mission kinds: `strike`, `dead`, `hunter`, `recce`, `bda`, `cas`, `harassment`, `helo_raid`, `deception`, `missile`,
`armed_recce`. Flight roles: `strike`, `dead`, `sead`, `hunter`, `escort`, `jammer`, `recce`, `decoy`, `helicopter`,
`missile`, and the support roles `awacs`, `tanker`, `elint`. Object kinds: `sam_site`, `aaa`,
`ew_radar`, `command_post`, `airfield`, `bridge`, `rail_junction`, `depot`, `power_plant`, `headquarters`, `troops`,
`factory`, `city`, and the nation's infrastructure (22 Oct 2026, the nation on the map N0): `refinery`, `pipeline_station`,
`substation`, `dam` (the grid: banned wherever `power_plant` is), `materials_depot`, `port`, `government`, `broadcast`,
`exchange`, and the supply lines' columns `convoy`, `train`, `column` (made by the loader, never written). Air defence systems (`defence.system`, checked by the loader): `osa`, `kub`, `s75`, `s75m2`, `s125`,
`s125m1`, `strela10`, `strela1`, `zsu234`, `s60`, `ks30`, `zu23`, `aaa`, `manpads`. Staff roles: `commander`, `planner`, `intelligence`, `sead`, `political`, `squadron_leader`.

Every aircraft number is a PLACEHOLDER; the doctrine's shape is sourced where a source exists and is a game value where
none does; the factions and the theatre are fictional.
