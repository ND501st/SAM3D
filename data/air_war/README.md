# data/air_war — the enemy air command

Loaded by `simcore/air_war/data.hpp` (`LoadAirWarData(data_dir, theatre_file, faction_file, ...)`), which also loads
`data/air_war/traits.json` and `data/gameplay/air_war.json` and cross-checks them. Design: `docs/air_war/`; research:
`docs/research/ENEMY_AIR_RESEARCH_2026-10.md`. The loaders are strict: unknown keys fail, numbers are range-checked,
every block names `source` and `confidence` (`tools/check_citations.py` checks the sources against `docs/SOURCES.md`).

| File | What |
|---|---|
| `theatre_pannonia.json` | The fictional country, its four fronts (entry points, the enemy's bases, the defender's fighter threat, where the player's battery deploys in a war on that front) and its objects (value, hardness, repair, mobility, emission, pre-war knowledge, air defence, C2 role). Positions in km east and north of `origin` |
| `faction_rebels.json` | Inferior: the Rava Liberation Front's improvised air wing |
| `faction_neutral.json` | Near peer: the Noric Republic's small air force |
| `faction_warsaw_pact.json` | Peer: the Warsaw Pact's air forces of the front |
| `faction_nato.json` | Superior: a composite allied tactical air force |
| `traits.json` | The eight traits and the 31 quirks with their named effects, signatures and our analysts' phrases |

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

Mission kinds: `strike`, `dead`, `hunter`, `recce`, `bda`, `cas`, `harassment`, `helo_raid`, `deception`. Flight roles:
`strike`, `dead`, `sead`, `hunter`, `escort`, `jammer`, `recce`, `decoy`, `helicopter`. Object kinds: `sam_site`, `aaa`,
`ew_radar`, `command_post`, `airfield`, `bridge`, `rail_junction`, `depot`, `power_plant`, `headquarters`, `troops`,
`factory`, `city`, and the nation's infrastructure (22 Oct 2026, the nation on the map N0): `refinery`, `pipeline_station`,
`substation`, `dam` (the grid: banned wherever `power_plant` is), `materials_depot`, `port`, `government`, `broadcast`,
`exchange`. Air defence systems (`defence.system`, checked by the loader): `osa`, `kub`, `s75`, `s75m2`, `s125`,
`s125m1`, `strela10`, `strela1`, `zsu234`, `s60`, `ks30`, `zu23`, `aaa`, `manpads`. Staff roles: `commander`, `planner`, `intelligence`, `sead`, `political`, `squadron_leader`.

Every aircraft number is a PLACEHOLDER; the doctrine's shape is sourced where a source exists and is a game value where
none does; the factions and the theatre are fictional.
