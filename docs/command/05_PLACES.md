# 5. The map: where the chain of command is

Data: `data/command/places_pannonia.json`, on the air war's theatre (`data/air_war/theatre_pannonia.json`; positions in
km east and north of its origin). Fictional places; game values.

## 1. The district

| Kind | Places |
|---|---|
| Small city | **Novo Varosd**: the 3rd Army's headquarters (the old cavalry barracks); the national air defence sector's command post (the bunker on Varos hill); the district national committee and civil defence; the hospital; the station; the Csepely engineering works with its militia battalion |
| Town | **Kesmark**, on the river: the bridges battery 21 defends; the garrison commandant's office (the old customs house); the town hall (the national committee, civil defence, the sirens); the public security station; the post office; the station and the military transport office; the sugar factory with its militia company |
| Villages | **Vrbice** (nearest the battery: the pub, the church tower that is the battery's observation post, the national committee's chairman); **Lazne** (the regiment's rear); **Horka** (the railway siding: the technical battery); **Dubnik** (the division's forest); **Polany** (the radar post above it) |
| Military posts in the countryside | Battery 21's command post and its four vehicles (moving with the battery); the regiment's command post in the Lazne woods; the division's main command post in the Dubnik forest; the division's air defence command post; the radar posts (P-40 above Polany, the P-19 company "Ludas"); Kecel air base |
| Countryside | The Lazne woods; the Vrbice crossroads (the commandant's traffic regulators); the Rava marshes |

A place in a city or town is **urban**: radio and wire suffer there (`utin_2018` §9.3). A place standing at a theatre
object takes its position from it (the battery's post and vehicles follow the battery as it moves; the bridges, the
radar posts, the division's air defence command post and the army's headquarters stand where the air war's targets are,
so the bombs that fall on them fall on the people there).

## 2. Who is where

Each post has a home place (`02` §2). Senior officers are there to be met: the regiment commander and chief of staff at
the regiment's command post, the division's chief of air defence at its command post, the army's chief of air defence in
the city, the garrison commandant in town.

## 3. Who is where when: the routines

| Routine | When (local) | Where | Who |
|---|---|---|---|
| The regiment's evening briefing | 19:00, an hour | regiment CP | the commander, the chief of staff, the duty officer, the chief of signals, the deputies, battery 21's commander (who drives there) |
| Battery 21's morning orders | 06:00, half an hour | the battery's CP | the battery commander, the four vehicle commanders, the sergeant major, the reload vehicle |
| Vehicle 21-1's crew told the day's orders | 06:30 | vehicle 21-1 | the vehicle commander and crew |
| The army's courier to the division | 04:00, 14:00 | army HQ → division CP → division AD CP | a direction (`utin_2018` §6.6) |
| The division's courier to the regiment | 06:00, 16:00 | division CP → division AD CP → regiment CP | a direction |
| The regiment's courier round | 07:30, 17:30 | regiment CP → battery 21 → technical battery → rear → regiment CP | a circular route |
| The sergeant major's run to the rear | 09:00 | battery → rear → battery | food, fuel, the post, the rear's news |
| The regiment commander's visit | 09:00 every other day | battery 21 | the battery commander |
| The political deputy's round | 14:00 every other day | battery 21 | the battery commander, the vehicle commanders, the sergeant major |
| The garrison commandant's call | 11:00 every third day | battery 21 | the battery commander |
| The state radio's news | 07:00, 19:00, 22:00 | everywhere with a receiver | everyone |

During a briefing or visit the people in it are at its place (`InfoNet::PlaceOf`): the battery commander at the
regiment's evening briefing is away from the battery for that hour.

## 4. For the Unreal map

The places are the content list for the district's level: a small city with barracks, a hill bunker, a hospital, a
station and works; a river town with two bridges, a customs house, a town hall, a police station, a post office, a
station and a sugar factory; five villages each with a church and a pub; forests, crossroads, marshes, hills with radar
posts. Each military post is a scene where its people can be met (`06_INTEGRATION.md` §6).
