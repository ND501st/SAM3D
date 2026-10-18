# 3. The means of signal

Data: `data/command/signal_means.json`. The kinds and their trade-offs follow `utin_2018` (operator-country textbook);
the speeds and handling times are SAM3D estimates until the period's norms are read (research note §7).

| Means | Kind | Carries | Speed and time | What it costs, what stops it |
|---|---|---|---|---|
| In person | in_person | papers in full | 90 s to talk a fact through (an urgent word: a quarter of that) | Only between people at the same place at the same time |
| Messenger on foot | messenger | papers | 5 km/h, 2 min handling, within 3 km | A runner between the vehicles and the CP |
| Despatch rider | despatch_rider | papers | 40 km/h on the road (×0.7 at night), 5 min handling; the route is the straight line × 1.3 | Sent at once by those who have riders (the regiment's three motorcycles, the division's courier platoon: `fm100_2_3`); lost where bombs fall on the road while it passes (then sent again an hour later) |
| Courier run | courier | papers both ways | on its schedule: 35 km/h between stops, 10 min at each | The direction (army → division, division → regiment) and the circular route (the regiment's round of its batteries): `utin_2018` §6.6. Lost where bombs fall |
| Field telephone | field_telephone | voice | a "Гранит" connection in 1 minute (`utin_2018` table 3.1), a routine one in 10; 1 min to talk | Both ends on the wire net, neither end's wire cut (a raid within 1.5 km of a post cuts its wire for 6 h); 2 % garbled; little to intercept |
| Town telephone | city_telephone | voice | 2 min (urgent) or 15 (routine) to connect | Between the town's offices and the posts with a line into the exchange; overheard (`utin_2018` §9.3) |
| Radio (voice) | radio_voice | voice | 45 s handling; 20 km (the R-123M's 4 m whip) | Out while jammed; 5 % garbled (repeated); **every call is heard by the enemy's intercept service** (logged: `InfoNet::RadioCalls`) |
| Early-warning net | warning_receiver | air warnings | 25 s; 60 km | The division's broadcast to the R-311 receivers (`fm100_2_3`): down the chain only, urgent only |
| Teleprinter | teleprinter | papers | 15 min (the auxiliary operations at both ends: `utin_2018` §3.1) | Division ↔ regiment CP over radio relay; priority and urgent only; needs the wire |
| Sirens | siren | the air-raid alarm | 10 s, 3 km | Civil defence's, in the town |
| Field post | field_post | letters, newspapers | two days | The men's letters and what the home front hears |
| State radio | broadcast | the news | at the bulletins (07:00, 19:00, 22:00) | What the state chooses to say; everyone with a receiver |

## Choosing between them

- **Urgent** (precedence 2: "Воздух", "Гранит"): the earliest arrival among every electrical means the pair shares, the
  sirens for an air-raid warning, a runner or a despatch rider.
- **Priority** (1): the wire if it is whole (telephone, town telephone, teleprinter): covert under the enemy's radio
  intercept (`utin_2018` §5); else the radio; else a despatch rider.
- **Routine** (0): the next contact (`04_PIPELINE.md` §3) if one comes within 30 hours; else as priority.
- **Precedence ladders** follow `utin_2018` §3.1: telegrams "Гранит", "Воздух", "Ракета", "Самолёт", "Обыкновенная";
  telephone passwords "Гранит", "Воздух", "Самолёт", "Связь-авария". The game uses three levels; the names are for the
  voices.
