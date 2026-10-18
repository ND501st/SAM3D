# 3. What the enemy believes: the intelligence picture

Code: `simcore/air_war/intel.hpp` (`IntelPicture`, `IntelEntry`, `IntelReport`); collection in
`simcore/air_war/campaign.hpp` (`PriorIntel`, `Collect`, `BeliefsFrom`, `Elint`). Research §4.

## 1. The picture

One entry per thing the enemy believes is there: a believed kind and label ("SA-8 battery near Kesmark"), a position and
its 1-sigma uncertainty, the belief that it exists, the believed working share, when it was observed and by what, its
worth, the air defence the enemy believes it has (radius, lethality, low and medium cover, radar), its record (strikes,
claims, losses credited) and the suspicion that it is a dummy. Ghost entries ("ghost-n") are things that aren't there.

The planner reads only this. Truth enters through reports.

## 2. Reports arrive late

| Discipline | When | What | Error | Latency |
|---|---|---|---|---|
| Pre-war survey | start | each object by its `prior_known` | fixed 200 m, mobile `prior_mobile_sigma_m` | — |
| Satellite | each pass (local hours), not in poor weather, half the chance under cloud | fixed objects, mobile ones by `satellite_detect` (halved under nets) | 150 m | `satellite_latency_h` |
| Air reconnaissance | survivors of recce flights | objects within `intel.recce_corridor_m` of the route, by the aircraft's recce quality (less under nets, in poor weather) | 300 m | `recce_latency_h` |
| ELINT | whatever radiates within `elint_reach_km` of the line: routine emission each hour and every second of a raid | the emitter | `elint_best_m + elint_k_m / sqrt(seconds)` | `elint_delay_h` |
| Radio intercept | the battery's voice traffic by its radio discipline | the battery | `comint_sigma_m` | `comint_delay_h` |
| Agents | `humint_per_day`, mostly what is near the front | a random object within reach; `humint_false` of them ghosts | `humint_sigma_m` | `humint_delay_h` |
| Pilots' reports | after every package | the target's state as claimed; dummies destroyed as SAM sites; radars that went silent under ARM attack as destroyed | | `pirep_delay_h` |
| Launch sightings | survivors near a defence that fired | where it is, and the losses credited to it | 700 m | `pirep_delay_h` |
| Losses to nobody seen | | "unidentified air defence" near the loss | 4 km | |

Merging: positions blend by inverse variance; a fix far outside both uncertainties means it moved, and the new place is
taken; belief in existence rises with each report; claims of destruction are believed in proportion to the analysts'
credulity.

## 3. The picture ages

Each hour a mobile entry's uncertainty grows by `intel.mobile_sigma_growth_mps` × 3600 (it may have driven off any time)
and the belief that it is there halves every `intel.mobile_confidence_half_life_h`; ghosts fade away. A fixed target's
believed damage heals at the rate the enemy assumes repair goes (`intel.fixed_function_recovery`). So a strike on a
mobile target must follow a fresh fix within hours or hit empty ground (Dugman 5, `wiki_model5`); a careful planner pays
for reconnaissance first, an "imagery man" insists on it.

## 4. Deception and EMCON (what the player does to it)

- **EMCON**: every second of radiation is an ELINT fix; a battery radiating 80% of the hour is known within a day, one
  silent and moving is not (test "a battery that radiates is found").
- **Moving**: the old fix ages; a strike aimed at it falls on empty ground (`hit_nothing`).
- **Dummies with decoy emitters** (roadmap 7.6, E7): an ELINT fix on the battery may land on the dummy or between them
  (`intel.ghost_fix_chance`, `vr_1985_plss`: active dummies on the same parameters); dummies photographed look like SAM
  sites unless an analyst sees through them (`intel.decoy_spot_skill`); a DEAD strike on a dummy is claimed as a kill.
- **Dummy radio traffic**: ghost fixes from the intercept service.
- **Switching off under ARM attack**: an ARM without memory misses; the pilots claim the radar destroyed; the "destroyed"
  battery fights again, and the enemy's staff loses face.
- **Nets**: halve detection on satellite passes and lower it on reconnaissance.

## 5. Claims

The pilots inflate by `bda_overclaim` (scaled by a random factor and by the staff's scepticism; a "paper tiger" adds
more). When a report later shows working something claimed destroyed:

- if the claim was false (it was never destroyed), **the truth comes out**: the intelligence officer and the planner lose
  reputation, the commander's scepticism rises;
- if it was repaired, the planner puts it back on the list ("repaired faster than expected").

## 6. The enemy's knowledge of the battery

`AirCampaign::EnemyKnowledgeOfBattery()` (0..1) is the XCOM-style measure: belief that it is there × how close the
believed place is to the real one. Our side gives the player its estimate each day ("the enemy probably knows the
battery's position: consider moving it").
