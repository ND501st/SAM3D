# 6. Reaction: lessons, politics, logistics, repair

Code: `simcore/air_war/campaign.hpp` (`ApplyOutcome`, `BeliefsFrom`, `Lessons`, `Daily`, `Events`, `Careers`, `Repair`,
`Relocate`). Research §3 (Linebacker II, Rolling Thunder, Lebanon 1983, Black Buck, Kosovo).

## 1. What changes after every raid

1. **The truth**: damage, wrecked objects (their time noted), dummies destroyed, rounds spent.
2. **The squadrons**: aircraft lost (decoy drones excepted), damaged aircraft out for `damaged_return_h`.
3. **Officers shot down**: killed or captured, replaced at once; a captured one is interrogated (our side learns his
   name and character).
4. **Prisoners**: pressure at home (by `pow_sensitivity`); interrogations reveal a quirk of the planner or commander.
5. **Civilian casualties**: the no-strike zone around towns widens (by `collateral_sensitivity`), support falls, the
   foreign press reports it.
6. **Beliefs** (`03_INTELLIGENCE.md` §2): claims, sightings, ELINT, photographs, all late.
7. **Lessons**: the planner's tactic memory; the SEAD lesson; stress, confidence and reputation for planner and
   commander; grudges against what shot them down.
8. **What our side saw**: the warning net's report of raids in the battery's sector; the patterns our analysts keep
   (time of day, corridors, feints, decoy waves, package sizes), which reveal the planner's quirks once they hold.

## 2. Each day

- **Politics**: pressure rises on days without results (`politics.pressure_per_quiet_day`) and eases with them; support
  falls with losses beyond the tolerance. The faction's **events** fire when their conditions hold: NATO's
  reinforcements on day 4, the alliance authorising strikes on the power grid under pressure, "fly above the light flak"
  after a day of heavy losses, HARM resupply, ceasefire talks; the Pact's "maximum effort", its second echelon, its
  losses review; the neutral's mediation, deeper raids authorised, a parliament shocked by two losses; the rebels'
  defector, ceasefire for the funerals, their martyrs.
- **Logistics**: replacements refill the squadrons' losses; ARMs are resupplied.
- **Careers**: superiors judge the day; relief, promotion (`02_AIR_STAFF.md` §4); intercepts tell our side of a change.
- **Stress** eases.
- **Our side's estimate** of how well the enemy knows the battery.

## 3. Every hour

- **Repair**: a damaged object regains working share at `1 / repair_h` an hour in the field (runways in hours, bridges
  in days, power stations in ten days). A wrecked one has nothing left to repair: it is out until its replacement comes
  (a temporary span, a battery from the reserve, new vehicles), `repair_h` after it was wrecked, and comes back at
  `repair.restored_function`, which the field repair then completes. No raid is offered to the player while the battery
  is being repaired. Air defences rearm a round an hour while working. The enemy assumes its own repair rate
  (`intel.fixed_function_recovery`); when reconnaissance shows something working sooner, it goes back on the list (the
  successive strikes "to prevent restoration" of `mai_tactics_vvs`).
- **Mobile units move** by their `relocate_h` and `relocate_km` (never closer than 8 km to the enemy's front); the player's
  battery by the posture's `relocate_every_h` or by `RelocateBattery` (its dummies move with it).
- **Reports arrive**; the picture ages.

**Planned** (the battery's war, `docs/campaign/02_WHAT_WE_DEFEND.md` §3, roadmap 13.5): objects are rebuilt by jobs
that need teams, materials, transport and time, each of them a target; this repair rule becomes a job's progress
with full means. Military units are still replaced from the reserve.

## 4. Resilience (the player's side)

Dispersal (mobile units move), redundancy (the warning nets and radars the PU-12 falls back on), hardness, repair,
dummies, nets, EMCON, radio discipline and dummy traffic are all in the model: each one changes what the enemy can find,
hit and claim, and so what it plans next.

## 5. The player's levers

`AirCampaign::Posture()`: emission share, EMCON discipline, radio discipline, dummy traffic, dummies and their decoy
mode, nets, emplacement, relocation rhythm, fighting silent. `RelocateBattery(position)`. The crew's skill tier
(`CrewTier`, rising one step per three aircraft the battery downs: pillar 5). The missions the player flies, and how.
