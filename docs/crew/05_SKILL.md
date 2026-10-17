# 05. Skill

Pillar 5: NPCs have skill levels, set by the player's progression and quests, and skill affects reaction times, speeds
and accuracy in every facet of activity.

## 1. Dimensions

Every behaviour an NPC has reads one or more of these. All values are **game values** (`data/crew/skill_levels.json`),
calibrated to the few published crew times (§3).

| Dimension | What it changes | Parameters |
|---|---|---|
| Perception | How soon a change on the seat's own instruments is noticed: a new blip, a lamp, the target in the TV, a callout heard | `perception_s` (mean delay), `miss_chance` (a callout not taken in, needing "Повторите") |
| Reaction | How soon a decided control action is done, per action | `reaction_s`, `reaction_jitter_s` (uniform ±) |
| Dexterity | Hand-wheel speed and how close it settles | `wheel_rate_factor` (× the tuning's aim rates), `tv_lag_s` (how far behind the target the TV wheels follow) |
| Accuracy | Errors in what is set and read | `aim_noise_deg` (TV aim), `range_error_m` (range by hand), `azimuth_error_deg` (azimuth onto a mark) |
| Procedure | Knowing the drill | `slip_chance` (an out-of-order action, corrected next), `omission_chance` (a step forgotten until reminded), `knows` (which drills the crewmate can do unaided: basic, all, including jamming, EMCON, faults) |
| Judgement | Decisions | `launch_timing_s` (how far from the ideal moment the launch is), `priority_error_chance` (takes the less threatening target first), `branch_error_chance` (picks a worse branch: radar when TV was right) |
| Communication | Speech | `callout_delay_s` (thinking before speaking), `ack_discipline` (chance an order is acknowledged at once) |
| Composure | Degradation under stress: night, jamming, being shot at, time pressure | `stress_factor` (multiplies delays and errors when stressed; 1 = unaffected) |
| Patience | How long he waits on another seat before reminding | `patience_s` |

## 2. Tiers

Named after the Soviet specialist classes (класс специалиста: без класса, 3rd, 2nd, 1st class, мастер; the naming is
sam3d's use of the standard grades, the numbers are game values).

| Tier | Name | reaction_s | jitter_s | perception_s | wheel × | tv_lag_s | aim_noise_deg | slip | omission | launch_timing_s | stress × | patience_s |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 0 | Recruit (без класса) | 2.4 | 1.0 | 3.5 | 0.55 | 0.9 | 0.25 | 0.06 | 0.03 | 8 | 1.8 | 12 |
| 1 | 3rd class | 1.4 | 0.5 | 2.0 | 0.70 | 0.6 | 0.17 | 0.03 | 0.01 | 4 | 1.5 | 9 |
| 2 | 2nd class | 1.1 | 0.4 | 1.6 | 0.85 | 0.45 | 0.11 | 0.01 | 0.004 | 3 | 1.3 | 7 |
| 3 | 1st class | 0.8 | 0.25 | 1.1 | 1.00 | 0.35 | 0.08 | 0.004 | 0.0 | 1.5 | 1.15 | 5 |
| 4 | Master (мастер) | 0.55 | 0.15 | 0.7 | 1.10 | 0.25 | 0.05 | 0.0 | 0.0 | 0.5 | 1.05 | 4 |

The drill master's reference performance (`06`) is computed at the Master tier with no jitter, no slips and no stress:
the best a real crew can do, not an impossible one.

## 3. Calibration targets

The tier table must reproduce these, measured by tests on full engagements with the whole crew at one tier:

| Measure | Value | Source | Tier it pins |
|---|---|---|---|
| Detection to launch | 26–40 s (26–34 s Osa; 27–39 s Osa-AK, AKM) | `nvo_2015`, `korovin_tiv_2010` | 1st class ≈ 27–30 s; 3rd class ≈ 38–40 s |
| Designation to launch, by the network chart | radar on target 10 s + capture 6–7 s + computer 3 s + preparation 15 s + launch 1.2 s ≈ 36 s | `voenizdat_osa_1991` Fig. 7.2 | 2nd class |
| New target after a kill | 40 s (excellent crew) to 120 s (poor) | `video_osa` (unverified) | Master ≈ 40 s, recruit ≈ 120 s |
| Deploy from a powered march | 35–50 s by crew skill | `video_osa` (unverified) | Master 35 s, 3rd class 50 s |
| March to combat, cold | ≤ 4 min including the GTA | `mon_2112_80` p11–12 | All tiers within 4 min except recruits |

Measured (16 October 2026, `simcore/tests/test_crew.cpp` "calibration"): a ready crew at one tier, run by its NPC
commander, against a fighter appearing inside the zone, mean of five seeds, first echo to launch: master 29.3 s, 1st
class 31.1 s, 2nd class 38.4 s, 3rd class 41.0 s, recruit 81.2 s. The 3rd class was tuned once (reaction 1.6 → 1.4 s,
perception 2.4 → 2.0 s, launch timing 5 → 4 s) to come near the band.

Re-measured 17 October 2026 over 20 seeds (five were too few: one bad run moved a tier's mean by 3 s), after each new
procedure's first step began drawing its own reaction (it had reused the last one): master 28.5 s, 1st class 31.4 s,
2nd 36.9 s, 3rd 41.1 s, recruit 85.8 s. The table is unchanged. The whole-scenario balance per tier is in `11`.

As built: reaction, jitter, slips, omissions and stress in `CrewMember`; wheel rates, TV lag, aim noise and the range
error by hand in the engagement (`AimCrew`); perception and callout delay in the callouts; the miss chance in hearing
reports; judgement as the launch timing on the decision controls (ПУСК), the commander's priority slips and his
hesitation at a branch; patience in the reminders. Stress comes from the crew's own radars being jammed, or a hostile
within 10 km with nothing tracked.

## 4. Per crewmate, per seat

- Each NPC is a person with a name and a **tier per seat** (a senior operator who is 1st class at his own seat may be
  3rd class at the deputy's). Moving a crewmate to another seat uses that seat's tier.
- **Stress** is the scenario's: night, jamming on the crew's own radars, the vehicle under fire, a target inside the
  zone with no track. Stressed values = base × `stress_factor` for delays and chances.
- Randomness comes from the scenario's seeded stream, so a run is repeatable (`ScenarioRunner` rules).

## 5. Progression and quests

- The **campaign** decides who is in the crew and at what tier (pillar 5). Early quests give a raw crew; progression
  trains them and brings better crewmates.
- Crewmates **gain experience** in the seats they work (completed drills, engagements, exams passed); enough
  experience promotes the seat's tier (`08` §4). Thresholds are game values.
- The player's own qualification (the exams passed per seat, `08`) unlocks the command half: first the vehicle, then
  the battery.
