# The enemy air command: design reference

Written 18 October 2026. This folder is the reference for the other side's air war in SAM3D: who plans the raids the
player meets, what they know, how they decide, how the raids are flown, what the enemy learns from them, and how
politics, leadership, losses and repair bend it all. The code is `simcore/include/simcore/air_war/` (namespace
`simcore::airwar`, umbrella `simcore/air_war.hpp`); the data is `data/air_war/` and `data/gameplay/air_war.json`; the
research is `docs/research/ENEMY_AIR_RESEARCH_2026-10.md`. When code and documents disagree, fix one of them in the same
change.

## The brief (project owner, 18 October 2026)

1. The year is 1980–85. The player's country is fictional and resembles Czechoslovakia, Hungary, Romania and Yugoslavia.
2. Four adversaries:
   - **inferior**: a small civil-war faction at the start of the game, light air power;
   - **near peer**: a small neutral nation with limited air power, helicopters and a light air force raiding along the
     front line;
   - **peer**: the Warsaw Pact, a deadly air force with deep strikes, a mix of package types and basic SEAD;
   - **superior**: NATO, complex packages, very long SEAD and DEAD campaigns, air superiority, EW, jamming and deception,
     a heavy strike campaign.
3. The enemy reacts to the player's actions, mission results, targets, politics, leadership, intelligence (human,
   imagery, satellite, ELINT/SIGINT), its commanders' personalities, successes and failures, destruction, repair and
   resilience.
4. The enemy's air warfare officers must feel like real people though they are never seen: they react, they have
   different skills, quirks and weaknesses, and these show in the missions they plan.
5. Everything integrates with the systems already built (scenarios and the runner, the crew, the battery and PU-12, C2,
   the EM layer, the missions menu, the debrief).

## How SAM3D reads it (decisions)

| Brief | Interpretation | Where |
|---|---|---|
| Four adversaries | One planner, four doctrines as data (planning cycle, methods, package templates, capabilities, intelligence, logistics, politics), each with its own aircraft, squadrons and officers | `01_FACTIONS.md` |
| Officers who feel real | A staff of people with eight traits, six skills, quirks with named effects on their choices, memory of tactics, grudges, stress, reputation, careers; replaced when they fall; seen by the player only through the raids and their own side's intelligence | `02_AIR_STAFF.md` |
| Intelligence | The enemy plans on a picture, never the truth: collection disciplines with latency, error and bias; mobile targets age fast; ghosts and dummies | `03_INTELLIGENCE.md` |
| Planning | A cycle per doctrine: tempo and apportionment, candidates, utility over ways to fly each one, packages from templates, approval, interference from above; every package keeps its reasons | `04_PLANNING.md` |
| Execution | Raids through the battery's sector become scenarios the existing runner plays; the rest are resolved abstractly by the same rules | `05_EXECUTION.md` |
| Reaction | Lessons from what they believe happened; politics, logistics, repair; careers | `06_ADAPTATION_POLITICS_LOGISTICS.md` |
| Integration | The raid scenario uses every system that exists; what each one contributes and receives | `07_INTEGRATION.md` |

## The loop

```
          ┌───────────── the enemy (AirCampaign) ─────────────┐
          │  staff (officers.hpp)   intel picture (intel.hpp)  │
          │        │ plan each cycle (planner.hpp)  ▲ reports   │
          │        ▼                                │ (late,    │
          │   air tasking order: packages           │  biased)  │
          └────────┬───────────────────────────┬────┴───────────┘
                   │ through the battery's     │ elsewhere
                   │ sector (pacing)           ▼
                   ▼                    resolve.hpp (abstract):
       raid_scenario.hpp: JSON for       routes through real defences,
       LoadScenario → ScenarioRunner     ARMs, aborts, bombs on what is
       (NPC crew, battery, PU-12, EM)    really at the aim point
                   │ mission_bridge.hpp:            │
                   │ releases, ARMs, emissions      │
                   ▼                                ▼
            PackageOutcome ──────► AirCampaign::ApplyOutcome:
                                   truth (damage), squadrons, officers shot down,
                                   pilots' claims, sightings, ELINT, photographs,
                                   lessons, grudges, politics, our side's INTSUM
```

## Contents

| File | What |
|---|---|
| `01_FACTIONS.md` | The four adversaries: doctrine, order of battle, intelligence, politics; the theatre |
| `02_AIR_STAFF.md` | The officers: roles, traits, skills, quirks and weaknesses, memory and careers; how the player perceives them |
| `03_INTELLIGENCE.md` | The enemy's picture: disciplines, latency, ageing, ghosts, dummies, claims |
| `04_PLANNING.md` | The planning cycle and the utility; packages; approval and interference |
| `05_EXECUTION.md` | The abstract resolution, the raid scenario and the mission bridge |
| `06_ADAPTATION_POLITICS_LOGISTICS.md` | Lessons, stress and reputation, politics and events, logistics, repair, resilience |
| `07_INTEGRATION.md` | Every existing system and how the air war uses it |
| `08_IMPLEMENTATION_PLAN.md` | What is built (phase A of the roadmap), what is next, done-when |
| `09_BALANCE.md` | The balance pass: every adversary over many seeds and two battery postures, the findings and the numbers they changed |

## Running it

```bash
cmake --build simcore/build --config Release --target air_war_demo
./simcore/build/air_war_demo nato 5 data out/air_war 1        # Windows: simcore\build\Release\air_war_demo.exe
./simcore/build/simcore_tests "air war"
```

In the game: the missions menu's **AIR WAR CAMPAIGN** box (1-4 a new campaign, C continue) opens the campaign screen;
see `unreal/README.md`, "Air war campaign". Campaigns are saved after every step (`air_war/save.hpp`).

The demo runs five days against NATO (or `rebels`, `neutral`, `warsaw_pact`), plays every raid through the battery's
sector as a scenario with the NPC crew, prints the journal (the enemy's mind, and in `[ours]` lines what our side
learned), the last intelligence summary and the whole truth about the enemy's staff, and writes each generated scenario
and `air_war_<faction>.json` to the output folder; `python tools/air_war_view.py out/air_war/air_war_nato.json` turns that into
a page with the enemy's mind beside what our side learned, day by day, and the reveal. Seven generated raids are in the game's missions menu ("Air war").
