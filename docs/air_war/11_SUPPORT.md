# 11. Support aircraft: AWACS, tankers, ELINT

Written 23 October 2026. The aircraft that fly no attack but shape every one: airborne early warning, air-to-air
refuelling and stand-off electronic intelligence. Code: `AirCampaign::SupportNow` and `Elint` (`campaign.hpp`), the
reach check in `Planner::Build` (`planner.hpp`), the fighter chance in `resolve.hpp`; data: the support types and
squadrons in `data/air_war/faction_nato.json` and `faction_warsaw_pact.json`, the `support` group of
`data/gameplay/air_war.json`. Research: `docs/research/AIR_WAR_MISSIONS_2026-10.md` §4.

## Who has what (1985)

| Faction | AWACS | Tankers | ELINT |
|---|---|---|---|
| NATO | E-3A (`naew_det`, 3 in theatre: the NATO AEW force's 18 from 1982, `afmag_1984_nato`) | KC-135A (`tankers`, 10; 60 % on day 1) | RC-135V/W (`rivet_joint`, 2) |
| Warsaw Pact | A-50 (`aew_det`, 2; in service 1984-85) | none in the front's air forces | Il-20M (`elint_det`, 2) |
| Neutral, rebels | none | none | none |

They are flown as orbits by the day, not in packages: each serviceable support aircraft flies `support.sorties_per_aircraft_day`
(1) long sortie; an orbit round the clock takes `awacs_sorties_per_orbit_day` (3) of an AWACS and `elint_sorties_per_orbit_day`
(2) of an ELINT aircraft; a tanker sortie refuels `receivers_per_tanker_sortie` (4) aircraft (Desert Storm's 4.2,
`gwaps_vol3` Table 16). `SupportLevel` holds the share of the day each orbit is up, the receivers the tankers can take and
the support sorties flown; the balance tool prints them.

## What they do

- **AWACS**: warning and vectoring for the escorts. The defender's fighter chance against a package without escort is
  cut by `awacs_fighter_cut` (0.5) times the share of the day the orbit is up, in the resolver and in the planner's
  expected losses alike.
- **Tankers**: every type has a combat radius (`radius_km`, published hi-lo-hi figures; `low_radius_share` 0.7 of it
  flown low). A flight whose way from its bases to the target and back is beyond its radius needs tanking: up to
  `refuelled_radius` (1.6) times the radius if the tankers can still take it this cycle, else that way is not flown. The
  package records its refuelled aircraft and the planner says so in its reasons.
- **ELINT aircraft**: with one up the faction's ELINT reaches its full depth and fixes at its best; without (its ground
  stations alone) it hears to `elint_reach_without` (0.6) of the depth and fixes `elint_sigma_without` (1.3) times
  coarser.

## Tests and the balance

The balance tool's "Support" line: NATO's AWACS on station all day on the nation's theatre, its tankers refuelling 6-13
aircraft a campaign, some 95 support sorties a week; the Pact's A-50 up a third of the day on the tuned theatre (two
aircraft) and all day on the nation's. `tests/test_air_war.cpp` covers the loaders; the support's effects are read in
the balance (`09_BALANCE.md` §10).
