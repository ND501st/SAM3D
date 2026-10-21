# 2. The air staff: officers who feel like real people

Code: `simcore/air_war/officers.hpp` (`Officer`, `AirStaff`); quirks `data/air_war/traits.json`; authored officers in
each faction file. Design references: the Nemesis system (`nemesis_gdc_2018`), XCOM 2's Chosen (`xcom2_wotc`), generals
with traits in strategy games (research §5).

## 1. Who plans the raids

| Post | Does | In the code |
|---|---|---|
| Commander | sets the tempo and the apportionment, approves or rejects each package, takes the blame and the credit; holds grudges | `Planner::Plan` step 1, `Approve`; reputation in `Careers` |
| Planner (operations) | picks targets and builds the packages; remembers how each way of doing it went | `BestPackage`, `Build`, tactic memory |
| Intelligence officer | keeps the picture; his skill and scepticism decide how much the pilots' claims are believed and whether dummies are seen through | `Deliver`, `BeliefsFrom`, `Collect` |
| EW/SEAD officer (where the doctrine has SEAD) | the suppression: its effectiveness and its timing with the strike | `coordination`, `supp_base` |
| Political officer (Warsaw Pact) | reports up the chain; vetoes strikes near towns | `Approve` |
| Squadron leaders | fly; their airmanship and nerve decide how a package fares and when it turns back; they can be killed or captured | `Resolve` (nerve), `ApplyOutcome` (shot down) |

## 2. Character

Eight traits, 0..1: **boldness** (accepts risk for results), **methodical** (suppresses and looks first, coordinates
well, learns carefully), **creativity** (tries untried ways, deceives, varies routes), **rigidity** (holds to doctrine and
plan, learns slowly), **ambition** (wants visible results), **scepticism** (doubts claims and agents), **vindictiveness**
(remembers who shot his men down), **composure** (keeps his head under losses). Six skills, 0..1: planning,
intelligence, SEAD, airmanship, leadership, politics. Stress lowers every skill in use (`Officer::Effective`), less for the
composed.

What each does to his choices, as the planner and resolver use them:

| Trait or skill | Effect |
|---|---|
| boldness | lower loss aversion; a higher approval limit; more tempo; more nerve before turning back |
| methodical | more SEAD per threat and DEAD/hunter share; better coordination; a heavier stale-target penalty |
| creativity | exploration of untried tactics (`utility.exploration`); more deception |
| rigidity | sticks to the doctrine's apportionment; learning rate down to a fifth |
| ambition | approves risky packages under pressure; promoted away when he succeeds |
| scepticism | pilots' claims believed less; rises when a claim is proved false |
| vindictiveness | the grudge's weight in a target's value |
| composure | less stress per loss, faster recovery |
| planning skill | the noise in his utility estimates (`utility.noise_at_zero_skill`) |
| intelligence skill | belief in claims, seeing through dummies |
| SEAD skill | suppression strength and timing |
| airmanship | evasion of the flights he leads |
| politics skill | resists interference from above; keeps his reputation |

## 3. Quirks and weaknesses

Thirty-one quirks (`traits.json`), each with named effects (`KnownQuirkEffects`), the **signature** the player sees in
the raids, and the **phrase** our analysts use once they have seen enough. Examples:

| Quirk | Effect | Signature | Weakness |
|---|---|---|---|
| dawn raider, dusk raider, night owl, daylight man | time-of-day preference | raids at that hour | predictable timing |
| creature of habit | the same entry corridor again | raids through one corridor | yes: an ambush on that corridor |
| pattern breaker | never the same way twice | every raid from a new direction | |
| feint artist | a feint pair from another entry minutes before | a pair shows and turns back | |
| decoy man | doubles the decoys | decoys first | |
| massed blows, drip feeder | package size and number | big raids then quiet, or pairs all day | |
| ARM hoarder | keeps ARMs back | little SEAD with ordinary raids | |
| leads from the front | flies the important missions | the lead talks like the boss | yes: he can be shot down |
| by the book | the doctrine's plan regardless | textbook raids | yes |
| paper tiger | inflates reports | claims far beyond our losses | yes |
| loose lips | his raids announced on open channels | our warning comes 15 minutes early | yes |
| ragged timing | suppression and bombers drift apart | SEAD minutes from the strike | yes |
| double tap | a second strike half an hour later | a smaller second raid | |
| ELINT believer | trusts every electronic fix | bombs where a decoy emitter was | yes |
| imagery man | no strike on a mobile target without fresh photographs | recce before every strike | |
| loss-shy, press-on | turn back at the first loss, or never | | loss-shy: yes |

Generated officers draw quirks by the faction's tier weights and the role (`roles`).

## 4. Memory, stress, reputation, career

- **Tactic memory** (`TacticMemory`): per entry point, height, time of day and mission kind, the share of aircraft lost
  and the success claimed, smoothed by his learning rate. Losses remembered make that way less attractive; success
  makes it more; creativity tries what hasn't been tried. A rigid planner keeps flying a corridor that costs him
  (Linebacker II, `dtic_linebacker`); a flexible one changes after one bad night.
- **The SEAD lesson**: losses to radar-guided defences on a package with little SEAD raise his SEAD per threat (`lesson_sead`).
- **Grudges**: every loss credited to a defence the enemy can attribute (a launch was seen) adds to the commander's and
  the leader's grudge against it; a grudge adds to the target's worth. "This one has cost us 3 aircraft: the commander
  wants it gone."
- **Stress** rises with losses beyond expectation, eases each day.
- **Reputation** with the superiors rises with claimed results, falls with losses beyond tolerance, with quiet days
  under pressure, and when a claim is proved false ("the SA-8 battery claimed destroyed is working again").
- **Career**: below `adapt.relieve_below` he is relieved and replaced by someone chosen **against his failing** (a
  careful man after a reckless one, a flexible one after a rigid one); an ambitious officer above `adapt.promote_above`
  is promoted away; a leader shot down is killed or captured. Every post is refilled at once (`AirStaff::Replace`).

## 5. Authored officers

Each faction has a hand-written staff with a background and a line of biography (more are generated as needed):

| Faction | Commander | Planner | Others |
|---|---|---|---|
| Rava Front | Ivo Kranjc, ex-airline captain, flies the night raids, keeps a list | Tomislav Babic, aero-club instructor, loose lips | Mirko Juric (informers) |
| Noric Republic | Brigadier Leopold Hainisch, cautious, loss-shy | Oberst Ernst Pichler, ex-display leader, dawn and low | Rudolf Moser (border service), Walter Steiner (helicopters, leads in person) |
| Warsaw Pact | Gen-Maj Viktor Sorokin, massed blows, by the book | Col Arkady Belyaev, dawn, the same corridor, to the minute | Voronin (ELINT believer), Akhmetov (MiG-25BMs, ARM hoarder), Kuznetsov (political, paper tiger), Tarasenko (Su-24, night), Gribov (MiG-23, grudges) |
| NATO | Maj Gen Thomas Halloran, suppression first, above the flak | Oberst Dieter Kessler, feints, never the same way twice, decoys | Pryce (photographs, sceptic), Duquesne (SEAD, ARM hoarder), Becker (Weasels, press-on, grudges), Ferraro (Tornados, dawn, low) |

## 6. How the player perceives them

The officers are never seen. The player knows them through:

1. **The raids**: timing, corridor, composition, feints, decoys, persistence.
2. **Our side's intelligence** (`AirCampaign::Intsum`, `DossierLine`, journal entries marked visible): the warning net's
   reports of every raid in the sector; our analysts' conclusions once a pattern holds (four raids or more): "the
   officer planning the raids, our analysts' 'the dawn raider': favours dawn raids"; interrogations of prisoners (name,
   role, a quirk); intercepts that the staff has changed; the daily estimate of how well the enemy knows the battery.
3. **The debrief's reveal** (`AirCampaign::EnemyMind`): the whole staff, their characters, quirks, stress, reputation,
   grudges and career notes; and the journal of every decision with its reasons.

A nickname comes first ("the feint artist"), the name only from a prisoner or an intercept.
