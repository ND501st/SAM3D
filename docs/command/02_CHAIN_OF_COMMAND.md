# 2. The chain of command around battery 21

Data: `data/command/chain_pannonia.json` (strictly loaded by `simcore/command/data.hpp`). Sources: the research note
§2-§5 (`utin_2018`, `fm100_2_3`, `uvs_rf_2007`, `flak11`, `basta_lm`); the names are fictional.

## 1. Levels

Every post has a level: how far down a fact must reach to get to it.

| Level | Who |
|---|---|
| 0 | The army and the national air defence (the army's chief of air defence and its intelligence; the national sector's duty officer) |
| 1 | The division (its commander, chief of staff, intelligence officer, chief of air defence and the duty officer of its command post); the fighter regiment |
| 2 | The regiment's commander and staff; the town's offices (the garrison commandant, civil defence, the police, the militia, the station's transport officer, the village council) |
| 3 | The battery commander; the regiment's radar post, medical post and technical battery |
| 4 | The vehicle commanders, the battery's sergeant major, the reload vehicle, the visual observation post |
| 5 | The crews |

A post's superior is never at a lower level (the loader checks it), and the chain never loops.

## 2. The posts

| Post | Holder | Where | Answers to; deals with | What they know first, what they do with it |
|---|---|---|---|---|
| Duty officer, national air defence sector CP | Major Laszlo Benedek | The bunker on Varos hill, Novo Varosd | the army's chief of air defence, the division's air defence CP, the fighter regiment, Kesmark's civil defence (sirens) | The raids coming: the first warning |
| Duty officer, fighter regiment | Captain Gabor Szilagyi | Kecel air base | the sector | Our own fighters in the air (for the batteries' identification, later) |
| Chief of air defence, 3rd Army | Colonel Milan Hrubes | The army's headquarters (the old cavalry barracks), Novo Varosd | the sector; the division's chief of air defence | The army's air defence orders |
| Air section, army intelligence | Lt Col Viktor Ondrejka | the army's headquarters | the chief of air defence | Intercepts of the enemy's air forces; the analysts' notes on the enemy's officers |
| Commander, 7th Motor Rifle Division | Major General Rudolf Kral | Division main CP, Dubnik forest | (the army's chief of air defence) | — |
| Chief of staff, division | Colonel Stefan Mraz | Division main CP | the commander | The garrison commandant answers to the division through him |
| Intelligence officer, division | Major Zoltan Kiss | Division main CP | the chief of staff; the garrison commandant | Questions the airmen our side takes prisoner |
| Chief of air defence, division | Colonel Antal Revesz | Division air defence CP | the division commander; the army's chief of air defence | Commands the SAM regiment |
| Duty officer, division air defence CP | Captain Petr Holub | Division air defence CP | the chief of air defence; the sector; the regiment's CP duty officer | Runs the early-warning net (R-311 receivers); the raids it saw |
| Commander, 15th SAM Regiment (Osa-AK) | Lt Col Josef Vrana | Regiment CP, the Lazne woods | the division's chief of air defence | Visits the batteries |
| Chief of staff (first deputy) | Major Imre Sallai | Regiment CP | the commander | Runs the CP and the evening briefing; the regiment's estimate of what the enemy knows of its batteries |
| Duty officer, regiment CP | Captain Pavel Liska | Regiment CP | the chief of staff; the division's warning net; battery 21's commander | Passes "Воздух" to the batteries; takes their reports |
| Deputy for political affairs | Major Ladislav Benes | Regiment rear, Lazne | the commander | The newspapers, the news, the post, morale |
| Deputy for armament | Major Dusan Kovac | Technical battery, Horka siding | the commander | Missiles, maintenance |
| Deputy for the rear | Major Ferenc Toth | Regiment rear | the commander; the station's transport officer | Fuel, food, transport, the field post |
| Chief of signals | Captain Milos Jelinek | Regiment CP | the chief of staff | The nets, the wire, the despatch riders |
| Radar post (P-40) | Senior Lt Tibor Nagy | above Polany | the CP's duty officer | The regiment's air picture (the PU-12's information chain, `docs/PU12_DEVELOPMENT.md`) |
| Medical post | Captain Ilona Farkas | Regiment rear | the deputy for the rear | The wounded |
| Technical battery | Captain Jan Polak | Horka siding | the deputy for armament; the station | Reload missiles |
| **Commander, battery 21** | Captain Andrej Novak | the battery's PU-12M | the regiment commander; the regiment's CP duty officer; the garrison commandant | Commands the four vehicles; covers the Kesmark bridges; the player's default post |
| Battery sergeant major | Staff Sgt Matej Kolar | the battery's CP | the battery commander | Fetches food, fuel and the post from the rear each morning, and the rear's news |
| Reload vehicle | Sgt Vaclav Strnad | the battery's CP | the battery commander | |
| Observation post, Vrbice church tower | Jr Sgt Robert Fiala | Vrbice | the battery commander | The battery's eyes when its radars are silent |
| **Vehicle commanders 21-1 .. 21-4** | Lt Martin Sykora, Sr Lt Lajos Varga, Lt Radek Cerny, Lt Peter Lukac | each vehicle's position | the battery commander | Posts the player may hold |
| **Vehicle 21-1's crew** | Sr Sgt Karol Hanak (deputy), Sgt Istvan Molnar (senior operator), Jr Sgt Tomas Dusek (operator), PFC Miro Benda (driver) | vehicle 21-1 | the vehicle commander | Posts the player may hold |
| Military commandant, Kesmark | Major Ondrej Blaha | the old customs house | the division's chief of staff; police, civil defence, the station, battery 21 | The town: roads and bridges open or cut, patrols; takes over the airmen the police and militia catch |
| Civil defence staff, Kesmark | Mrs Eva Sedlackova | the town hall | the commandant, the police, the sector | The sirens; the town's damage |
| Public security, Kesmark | Police Captain Jozef Hornak | the police station | the commandant, the militia, Vrbice | Picks up the parachutists the villages report |
| Militia company, sugar factory | Bela Kocsis | the sugar factory | the police, the commandant | Guards the works and the bridge's town end (`basta_lm`) |
| National committee, Vrbice | Frantisek Zeman | Vrbice | the police | The village's telephone and its gossip |
| Military transport officer, station | Captain Igor Danek | Kesmark station | the commandant, the regiment's rear, the technical battery | Missiles and fuel by rail |
| Militia battalion, Csepely works | Karel Prochazka | the works, Novo Varosd | the army's chief of air defence | The city's works |

## 3. What is not modelled yet

- The other four batteries of the regiment, and vehicles 21-2..21-4's crews (their commanders are there).
- The motorised rifle regiment a battery covers in the field (the Kesmark bridges are an object, not a unit).
- Replacing a post's holder when they fall (the air war does it for the enemy's staff: `docs/air_war/02_AIR_STAFF.md` §4).
- The player's career moving them between posts (`docs/crew/08_TRAINING_AND_ASSISTANCE.md`), and the alternate command
  post under a deputy when the KP is lost (`utin_2018` §1).
