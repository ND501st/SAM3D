# 6. The crew as people: needs, duty, progression, and whom the player plays

Planned code: `simcore/campaign/people.hpp`. Data: the battery's roster `data/crew/battery_roster.json`, needs and their
effects `data/gameplay/crew_needs.json`. Roadmap phase 19.

## 1. Why

The crew pillars (`docs/crew/README.md`) put teamwork first and give every NPC a skill that changes how fast and how
well they work. Across a campaign that skill also depends on whether the person has slept, eaten, lost a friend, or
just shot down the aircraft that bombed their town. Managing the battery's people is part of the battery commander's
strategy: a battery is ready only as far as its people are. Because each person keeps their own state, the player can
take different members of the battery across missions and get a rounded experience of the battery: the driver's
night march, the operator's watch, the TZM crew's reload, the battery commander's planning (§6).

## 2. The people

Every member of battery 21 is a person with a name (the chain of command already names the battery's officers and
21-1's crew, `data/command/chain_pannonia.json`):

| Group | Who (the textbook's battery, `voenizdat_osa_1991` p226) |
|---|---|
| Command | The battery commander; the PU-12M's crew (`docs/PU12_DEVELOPMENT.md`) |
| Four launcher crews | Commander, deputy commander, senior operator, operator, driver (phase 6.1) |
| Support | Two TZM crews, the maintenance crew (four, with the 9V210M3), the supply section, the sergeant major |
| MANPADS section | Three launchers' gunners |
| The observation post | On the Vrbice church tower (`docs/command/02_CHAIN_OF_COMMAND.md`) |

Each person carries:

| State | What | Built on |
|---|---|---|
| **Qualifications** | The seats they are qualified for, and their specialist class in each (the tiers, `docs/crew/05_SKILL.md` §2) | `crew[].skill`, the exams (`docs/crew/08_TRAINING_AND_ASSISTANCE.md`) |
| **Experience** | Per seat: drills, engagements, exams | Pillar 5's progression |
| **Condition** | Fatigue and sleep debt; hunger; morale; health (§3) | New |
| **Place** | Always somewhere: seated, in a hatch, on the ground, in a tent, at the regiment's rear | `crew_mobility.hpp` (8.14) |
| **Duty** | On watch at a post, resting, at an activity (`07`), away | New |
| **Knowledge** | What has reached this person (first person) | `crew_knowledge.hpp`, `command/info_net.hpp` |

Today the campaign keeps one crew tier for the battery (`AirCampaign::CrewTier`), and only 21-1 is crewed in a fight.
The other launchers are moved, not crewed (7.5, 8.9). The roster makes every person real. The other vehicles' people
fight through the battery's fire model until their vehicles are crewed in full (Later).

## 3. Needs, and what they do to performance

Needs change a person's **effective tier** for the hour. Each need multiplies the dimensions the skill model already has
(`docs/crew/05_SKILL.md` §1), the same way the `stress_factor` does: a 1st-class operator who has not slept for two
days works like a 3rd-class one. All the numbers are game values until research step 19.0 finds sourced figures:
public human-factors studies of sleep loss and vigilance, and the period's field regulations on the daily routine
(распорядок дня), rest on duty and meal norms, operator-country documents first.

| Need | Grows with | Eases with | Hits hardest | At the extreme |
|---|---|---|---|---|
| **Sleep** (fatigue, sleep debt) | Hours awake, night work, alarms that break sleep | Sleep: best in a dugout or tent, worse sitting in the vehicle, little with the radars and the turbine running | Perception (a blip noticed late), omissions, patience; the night watch | A person on watch may doze: a callout missed, a check-in unanswered |
| **Food and water** | Hours since a meal; heavy work (digging, loading missiles) | Meals: the field kitchen's hot meal brought on the sergeant major's run, or dry rations; the textbook's 2-hour meal halt on the march (p. 236) | Composure; the speed of physical jobs | Strength gone: jobs at places (`07`) take longer |
| **Morale** (the person's, and the battery's) | Losses in the battery or the regiment; near misses and ARM attacks; bad news from home; fatigue and hunger; orders that seem pointless | Kills seen; an enemy wreck nearby; letters; good news; rest and hot food; the commander sharing the danger; the political deputy's round | Communication (acknowledgement discipline), judgement, initiative when command is cut (`08` §3) | Slack watch; slow, grudging work |
| **Health** | Wounds (bombs, ARMs, a vehicle hit), cold, exhaustion | The medical post (Captain Farkas, the regiment's rear), evacuation, replacement | Everything | Out of the battery; a replacement comes (P8) |

The needs are visible the way everything is, through people (pillar 3): a vehicle commander reports "the crew has not
slept for two days", acknowledgements come slower, the sergeant major says the kitchen did not come up. They show on
no gauge.

## 4. Duty: who is at the posts, who rests

The battery cannot keep everyone at their posts for days. Readiness levels (Nos. 1-3: radios, telecode and automation
ready to receive designations at all three) and the duty subunit, controlled from the senior duty command post, are in
the regulation (`pvo_bu_ix_1984`, `docs/research/CREW_COMMS_2026-10.md`). A veteran's account gives the night: the
command post's duty officer called each vehicle in a whisper on the field telephone, and crews wore their headsets
with the handset tucked to the ear so as not to miss the check (`airbase_ek32_2012`).

- **The roster** sets who is on watch at each readiness and for how long: a duty vehicle at its posts, the others'
  crews resting beside their vehicles. How many people each readiness level keeps at the posts is research step 19.0.
  Until then it is a game value in the roster.
- **Readiness takes time to reach, and the time is computed.** From where each needed person is (asleep in a tent,
  eating at the PU-12M, on the hull), their legs to their seat (`crew_mobility.hpp`: out of the tent, up the wheel, in
  through the hatch) plus the time to wake, plus the equipment's own state (`07` §3). Nobody teleports.
- **The roster is the battery commander's to set**, inside the readiness the regiment orders. It goes out in the morning
  orders (06:00, `docs/command/05_PLACES.md` §3) and changes by order when the situation does.

## 5. Progression

- **Per person, per seat**: experience from the drills, engagements and exams that person took part in. The specialist
  class (the tiers) is awarded at thresholds and by exam (`docs/crew/05_SKILL.md` §5). Today it is one tier for the
  campaign, rising one step for every three aircraft the battery downs (`docs/air_war/06_ADAPTATION_POLITICS_LOGISTICS.md`
  §5).
- **Cross-training**: a person may qualify on more than one seat. That is who takes the seat when its crewman falls.
- **People move up**: a senior operator becomes a vehicle commander; a vehicle commander becomes the deputy battery
  commander (posts changing hands, P8).
- **Losses take experience with them.** Replacements come as recruits. A seasoned crew is a strategic asset that
  scheduling (`07`) and doctrine (`08`) either preserve or spend.

## 6. Whom the player plays

The player has a **main character**, whose career is the career mode (`docs/crew/08_TRAINING_AND_ASSISTANCE.md` §4). The
player may also **take any fit member of the battery** for a mission or an activity, as T takes a seat in a mission
today (8.8):

- the person taken keeps their own condition, place, knowledge and progression. The player knows only what that
  person knows (the first-person rule holds for whoever is played);
- the person left becomes an NPC again, with the same state;
- only someone awake (or woken), at hand and not wounded can be taken. Taking a seat means walking to it
  (`crew_mobility.hpp`);
- experience goes to the person played; the main character's career moves only by what the main character does.

Across a campaign the player can drive the march, sit the operator's night watch, load with the TZM crew, and command
the battery at dawn, each time as the person who would really be there, tired or rested.

## 7. Built on

The skill tiers and their stress multiplier (`docs/crew/05_SKILL.md`); crew bodies and the walk (`crew_mobility.hpp`,
`docs/crew/13_OUT_OF_THE_VEHICLE.md`, whose §4 already names rest, meals and guard as jobs at places); the routines
(the morning orders, the sergeant major's run with food and post, the political deputy's round:
`places_pannonia.json`); the medical post and the deputy for the rear (`docs/command/02_CHAIN_OF_COMMAND.md`); the
field post and the news as morale's inputs (`docs/command/03_MEANS.md`); the crew's voices (8.10).
