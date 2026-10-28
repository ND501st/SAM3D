# Briefing: building up the nation, its industry and its forces (proposal, 22 October 2026)

For the project owner. A proposed outline of the Pannonian Socialist Federal Republic as the battery's war needs it: the
country's map, its industry and infrastructure, its command structure, its national air defence, air force and ground
forces, their people, and their support equipment and assets. The air defence follows your list; your decisions so far
are in §10; everything else is my proposal. Section 11 is the research that would source it. It is built first, by
the roadmap's nation-on-the-map track (N0-N6, prioritised 22 October 2026), and phase 13 (13.7) adds its roles,
rebuilds and fuel. **In the data since N2** (22 October 2026): the country's map (`data/maps/pannonia.json`), the
assets of §3-9 placed on it (`data/air_war/theatre_pannonia_nation.json`, `docs/COUNTRY_MAP.md`) and our order of battle
(`data/command/oob_pannonia.json`).

## 1. Bottom line

- **Decided** (22 October 2026, §10): the S-75 is the **S-75M2**; **no MiG-29s** in 1980-85 (an option for a later
  campaign), a squadron of **MiG-23MLs** in their place; the Strela-10 platoons become **Strela-1 (SA-9)** platoons and
  **the Kub stays**; the Osa regiment has **five batteries**; **two services**, the National Air Defence and the Air
  Force; **the KS-30s stay**; the oil comes **mainly from the big Warsaw Pact neighbour**, a smaller share from the
  neutral neighbour, so **a war with the Pact is a war short of fuel**; the army is **two motor rifle divisions of three
  regiments and a tank brigade**, and **the Kub battery is the tank brigade's air defence**.
- **Decided**: the capital stands on the Rava about 60 km north of Kesmark (decision 5). **Still to come**: its name,
  which you will give.
- **The country** grows from today's 40 theatre objects and one 40 km district to a whole country of about 300 by 300
  km on the same coordinates, with industry and infrastructure as networks (`02_WHAT_WE_DEFEND.md`) (§2-3).
- **Command** (§4): a Ministry of National Defence and General Staff over three branches: the Ground Forces (the 3rd
  Army), the National Air Defence (the fighters, the S-75M2, S-125M1 and KS-30 units, the radio-technical troops) and
  the Air Force (fighter-bombers, reconnaissance, training, transport, helicopters). The Interior ministry's border
  guards and the party's militia stand beside them. Every post of today's chain of command has a place in it.
- **The forces** (§5-7) follow your list for the air defence: about 6 S-75M2 and 10 S-125M1 fire units, five Osa
  batteries, a Kub battery, six regimental batteries of Strela-1s and Shilkas, Strela-2 MANPADS, and many ZU-23, S-60
  and KS-30 guns.
- **Personnel** (§8): officers, career NCOs and conscripts, with twice-yearly intakes that bring recruits in and take
  trained men out, and reservists in war. Establishments per unit are given where sources give them, and as estimates
  where they don't.
- **Support** (§9): every unit's radars, command posts, reloaders, refuellers, power, maintenance and transport, each a
  thing the enemy can see and strike.

## 2. The country and its map

Today's theatre (`data/air_war/theatre_pannonia.json`) puts objects from about 125 km west to 70 km east and 90 km south
to 100 km north of its origin (46.9 N, 19.7 E), and the enemy's entry points out to 185 km. The proposal keeps those
coordinates and fills the country to its borders:

| Region | Where (km from the origin) | What is there |
|---|---|---|
| **The central plain** | The origin's district | Kesmark and its bridges (the test range), the 7th Division's area, Kecel air base |
| **The capital** (decision 5; name to come) | On the Rava, about 60 km north of Kesmark | Ministries, the General Staff, the state radio and television centre, the main telephone exchanges, the National Air Defence's central command post, an airport; the heaviest air defence |
| **The industrial belt** | Novo Varosd and the Csepely works (−30, 40), along the rail line to the capital | Engineering, the vehicle works, the arms and ammunition plant, the radio-electronics plant, the aircraft repair works at an air base |
| **The northern hills** | Towards the Pact border (about +130 km north-east) | Hradec, the hydroelectric dam on the upper Rava, the main oil pipeline's border station, radar posts on the crests, the northern group |
| **The western uplands** | Towards the NATO border (about −170 km west) | The western group, radar posts |
| **The south-west border hills** | Towards the neutral neighbour (about −120 to −170 km) | Saldanj, the smaller oil line's border station, the border battalion and its artillery, the Lenta bridge (already in the theatre) |
| **The Rava valley in the south** | South of Kesmark to about −110 km | Ralje, the southern group, the rebels' country |

**The map at two scales.** The whole country as a strategic map at about 1 km (`data/maps/pannonia.json`: the ground,
the rivers, the main roads and railways, the pipelines, the towns, the woods), and 40 km districts at full detail
generated where the battery is (`terrain_map.hpp`, the test range's method, deterministic for each seed: roadmap 15.6).
The places of our chain of command (`data/command/places_pannonia.json`) stay as they are and are joined by the new
posts.

## 3. Industry and infrastructure

The target systems of `02_WHAT_WE_DEFEND.md` §1, filled in. Numbers are proposals and game values:

| System | Proposed |
|---|---|
| **Power** | Paksany thermal station (in the theatre), a second thermal station near the capital, the hydroelectric dam in the north; substations and lines between them |
| **Fuel** | The oil pipelines and the refinery (§3.1); the state's reserve depots; fuel depots per region (three in the theatre); the military's fuel depots at the air bases and the army's rear |
| **Munitions** | The arms and ammunition plant in the industrial belt; the central and the services' depots; the SAM units' technical battalions (missile assembly, and fuelling for the S-75M2's liquid-fuelled missiles); imports by rail (§3.2) |
| **Transport** | Main rail lines and marshalling yards (Cegled junction in the theatre), the Rava bridges (road and rail at Kesmark and the capital, two more), main roads, a river port |
| **Command and communications** | The capital's exchanges, trunk cables and radio relay; the command posts of §4 (the army's and the division's are in the theatre); the National Air Defence's posts (the bunker on Varos hill is in the theatre) |
| **Warning** | The radio-technical brigade's radar companies (§5), the army's radar posts (the P-40 and P-19 in the theatre) |
| **Production and the state** | The Csepely engineering works (in the theatre), the vehicle works, the aircraft repair works, the radio-electronics plant, the chemical works; the ministries; state radio and television |
| **The rebuild means** | Railway troops, engineers with a pontoon park, power line crews, materials depots (`02` §3) |

### 3.1 Oil and fuel (decision 9)

Most of the country's crude oil comes by **the main pipeline from the big Warsaw Pact neighbour** in the north-east. A
**smaller share comes from the neutral neighbour** in the south-west, by a smaller line and by rail. Both feed the
refinery on the Rava south of the capital. The state's reserve depots hold some weeks of peacetime use (a game value),
and a war burns fuel several times faster: sorties, marches, generators, the ATZ's runs.

| War | The oil | What follows |
|---|---|---|
| **The Warsaw Pact** | The main pipeline stops on the first day | The country lives on its reserves and the neutral's share. Rationing from the start: fewer sorties, shorter marches, generators only when the grid is down, the ATZ's loads cut. The reserve depots, the refinery and the neutral's line become the targets that matter most (the Pact weights depots 0.9) |
| **The Noric Republic** | The smaller line stops | A tighter balance; the main pipeline still flows |
| **NATO** | Both may flow, but the refinery, the depots and the pipeline are interdiction targets (its phase 2) | Shortages follow its strikes |
| **The rebels** | Both flow; the southern depots and lines are within their 80 km | Local shortages after raids and sabotage |

At the battery, a shortage is the ATZ coming half full, the order to stay on the grid rather than the diesel, the
half-tank rule biting sooner (7.4), and a regiment that moves its batteries less.

### 3.2 Arms from abroad

As a Soviet-equipped state, the country gets its missiles, spares and much of its ammunition for its Soviet systems by
rail through the same Pact neighbour. In a war with the Pact that stops too, and the S-75M2's, S-125M1's, Kub's and
Osa's missiles are what is in the depots and the technical battalions, with little the arms plant can make. In the other
wars resupply can come as an event (an ally's shipment), by the rail line the enemy can strike.

## 4. Command structure

### 4.1 The national level and the services (decision 7: two services)

```
Defence Council (the state's leadership)
└── Ministry of National Defence ─── General Staff (the capital)
    ├── Ground Forces ──── 3rd Army (Novo Varosd)
    │                      ├── 7th Motor Rifle Division (Dubnik forest) ── its regiments, the 15th SAM Regiment (Osa)
    │                      ├── a second motor rifle division ── its regiments, the S-60 regiment
    │                      ├── a tank brigade ── its Kub battery
    │                      └── army troops: artillery, engineers, signals, the rear; the army's chief of air defence
    ├── National Air Defence ── central command post (the capital)
    │                      ├── 1st and 2nd Air Defence Sectors (the 1st's post is the bunker on Varos hill)
    │                      ├── the fighter aviation division: three fighter regiments
    │                      ├── the S-75M2 brigade, the S-125M1 regiment(s), the KS-30 regiment
    │                      └── the radio-technical brigade
    ├── Air Force ──────── headquarters near the capital
    │                      ├── fighter-bomber regiment, reconnaissance squadron, training regiment
    │                      ├── transport squadron, helicopter regiment (in support of the army)
    │                      └── airfield support battalions; the aircraft repair works
    └── Rear services ──── central depots, the fuel service, the military transport offices, the railway troops
Ministry of the Interior: border guards, police; civil defence (with the national committees)
The party: the workers' militia in the factories
```

**Why the fighters sit with the National Air Defence**: our chain already has the fighter regiment's duty officer
working to the national sector's duty officer, who directs the interceptions. The proposal reads this as the
Czechoslovak pattern of a national air defence holding its own fighter divisions, beside an air force holding the
fighter-bombers. That reading is to be confirmed (research item 1). If it doesn't hold, the fighters move to the Air
Force and the sectors keep their operational control.

### 4.2 Where today's chain sits in it

| Today's posts (`data/command/chain_pannonia.json`) | In the structure |
|---|---|
| The national sector's duty officer (Maj Benedek, the bunker on Varos hill) | National Air Defence, the 1st Sector's command post |
| The fighter regiment's duty officer (Capt Szilagyi, Kecel) | National Air Defence, the fighter division's 1st Fighter Regiment, under the 1st Sector's control |
| The 3rd Army's chief of air defence (Col Hrubes) and its intelligence's air section (Lt Col Ondrejka) | Ground Forces, the 3rd Army |
| The 7th Motor Rifle Division (Maj Gen Kral, Col Mraz, Maj Kiss, Col Revesz, Capt Holub) | Ground Forces, the 3rd Army |
| The 15th SAM Regiment (Lt Col Vrana and his staff) and battery 21 (Capt Novak) | The 7th Division's air defence: five batteries, 21 to 25 |
| The garrison commandant, civil defence, the police, the militia, the national committee, the station's transport officer | The division's garrison commandant; the Interior ministry; the party's militia; the local state; the rear services' transport offices |

### 4.3 Command posts and their alternates

Every headquarters from the regiment up has a main command post and an alternate one under a deputy, which takes
over if the main post is lost (`utin_2018`; `08_COMMAND_DOCTRINE_AND_REACH.md` §3). The National Air Defence's sectors
are the hinge between the services: they see the raids first, control the fighters, give the SAM units their fire
control and warning, and pass the warning to the army's air defence. Each post is a place with people, links and an
alternate, and each is a target.

## 5. The National Air Defence (service)

| Unit | Strength | Each with | Where (proposal) |
|---|---|---|---|
| **Two Air Defence Sectors** | The 1st (centre, west and south), the 2nd (north and east) | A command post with its plotting, the warning, the fighter control and the SAM units' fire control; an alternate post | The 1st's on Varos hill (in the theatre); the 2nd's in the northern hills |
| **Fighter aviation division** | Three fighter regiments (§5.1) | Its own command post beside the sector posts | Kecel, Tiszavar and a northern base |
| **S-75M2 brigade** | About 6 fire units (a guidance radar and six launchers each; in Soviet usage a fire unit is a battalion, дивизион) | Guidance radar (SNR-75 family), acquisition radar (P-12/P-18 family) and a height finder, six launchers, transloaders and missile transport, a power station; the brigade's technical battalion assembling and fuelling missiles; the brigade's command post | Three round the capital, one over the industrial belt (the Varosd site in the theatre), one at Paksany, one on the north-eastern approach |
| **S-125M1 regiment(s)** | About 10 fire units | Guidance radar (SNR-125M1), acquisition radar (P-15 family), four launchers of up to four missiles, transloaders, power; a technical battalion | Three in the capital's inner ring, the air bases (Kecel is in the theatre; Tiszavar), the Kesmark bridges, Paksany, the refinery, Novo Varosd, the dam |
| **KS-30 regiment** (decision 8) | 130 mm guns, 3-4 batteries | Gun-laying radars and directors (research item 4), heavy tractors, ammunition trucks | Static round the capital: the capital's heavy layer, older guns the state held on to |
| **Radio-technical brigade** | About 10 radar companies in three or four battalions | Long-range, height-finding and low-altitude radars (P-14, P-35/P-37, PRV-11/13, P-18, P-19 families: research item 5) | Along the four borders and round the capital (the Bugac battalion is in the theatre) |

### 5.1 The fighters

| Unit | Aircraft (proposed numbers) | Base | Role |
|---|---|---|---|
| **1st Fighter Regiment** | MiG-21bis and MiG-21MF, about 36, with MiG-21UM trainers | Kecel (in the theatre) | Interception under the sectors' control; escort |
| **2nd Fighter Regiment** | The same | A northern base | The same, facing the Pact border |
| **3rd Fighter Regiment** | Two squadrons of MiG-21bis (about 24) and one of **MiG-23ML** (about 12, with two or three MiG-23UBs: decision 2). In a later campaign's option the MiG-23 squadron re-equips with MiG-29s | Tiszavar (in the theatre) | The MiG-23MLs are the best fighters the country has, held for the capital and the heaviest raids |

Our MiG-23ML has its own basic model in our colours (`mig23ml`, N3), beside the Pact's escort fighter (`mig23mld`): in a
war with the Pact both sides fly MiG-23s: one more reason a battery must know which of ours are up and trust its IFF (roadmap 17.2).

## 6. The Air Force (service)

| Unit | Aircraft (proposed numbers) | Base | Role |
|---|---|---|---|
| **Fighter-bomber regiment** | Su-17 family (exported as the Su-20 and Su-22), about 24-30 | A southern base | Strikes on the rebels' strips and the neutral's forward bases; close support |
| **Reconnaissance squadron** | Su-22s with pods, or MiG-21Rs, about 8 | With the fighter-bombers | Our picture of the enemy's bases and front (roadmap 16.6) |
| **Training regiment** | L-39 Albatros, about 30, with an armed squadron of about 12 | A southern or central base | Training; light attack against the rebels |
| **Helicopter regiment** | Mi-24 (about 12-16), Mi-8 and Mi-17 (about 30), Mi-2 (about 20) | With the army's formations | Close support, transport, liaison, rescue |
| **Transport squadron** | An-26 (about 8), An-2 (about 10) | The capital's airport | Transport; the An-2s are where the rebels' crop sprayers came from |
| **Airfield support battalions** | One per air base | §9 | Fuel, power, munitions, runway repair, the airfield's radars and beacons |
| **Aircraft repair works** | At an air base in the industrial belt | The overhauls | A production target (`02` §1) |

## 7. The Ground Forces (service)

| Formation | Decided (decision 6) |
|---|---|
| **3rd Army** (in the chain) | The field army's headquarters in Novo Varosd, with its chief of air defence |
| **7th Motor Rifle Division** (in the chain) | Two motor rifle regiments and a tank regiment, each with its AA battery; the 15th SAM Regiment; an artillery regiment; engineer, signals, reconnaissance, maintenance, medical and transport battalions |
| **A second motor rifle division** | The same, with the S-60 regiment as its air defence |
| **A tank brigade** | The army's reserve. Its air defence is the Kub battery (decision 6), with its own ZU-23s and Strela-2s |
| **Army troops** | An artillery brigade, an engineer brigade with the pontoon park (the rebuild means, `02` §3), a signals regiment, the army's rear base and depots |
| **Border guards** (Interior ministry) | Along the four borders (the south-western border battalion is in the theatre) |
| **Territorial defence and the militia** | Guards for the works, the bridges and the towns, with ZU-23s and Strela-2s (the factories' militia is in the chain) |

### 7.1 The troop air defence

| Unit | Strength | Each with | Where |
|---|---|---|---|
| **15th SAM Regiment (Osa-AK)** (decision 4) | **Five batteries (21-25)**, a headquarters battery with the PU-12M and the regiment's radars, a technical battery (`flak11`, as the chain already has it) | Per battery: the PU-12M, four 9A33s, three MANPADS launchers, two TZMs, the maintenance and ZIP-2 vehicles (`voenizdat_osa_1991` p226) | With the 7th Motor Rifle Division (in the theatre: batteries 21 and 22; 23-25 to add) |
| **S-60 regiment** | 57 mm, about 4 batteries of six guns | Gun-laying radars (SON-9 family) and directors, tractors, ammunition trucks, a regimental command post | With the second motor rifle division |
| **Kub (2K12) battery** (decisions 3 and 6) | 1, kept from the theatre | Its radar vehicle and launchers, reloaders and power (research item 4) | **The tank brigade's air defence.** The brigade is the army's reserve in the east, so in peace the battery stands near Tiszavar (where the theatre has it) and covers the air base too; in war it goes where the brigade goes |
| **Regimental AA batteries** | 6, one per regiment of the two divisions: a Strela-1 (SA-9) platoon of four and a Shilka platoon of four (24 of each) | A PU-12 per battery (the PU-12's other users, `pvo_bu_ix_1984`); the Shilkas' radars (Gun Dish: band still unsourced, E1) | The two Strela-10 platoons in the theatre become two of these platoons (decision 3); the pattern is the NATO view of the Soviet regiment (`fm100_2_3`), to confirm |
| **MANPADS** | 9K32 Strela-2 (2M): about 150-200 launchers | Sections in the battalions and with the SAM batteries | Everywhere the troops are |
| **Light AA guns** | ZU-23-2: 200-300 mounts | Trucks or towed; optical | The regiments, the airfields, the bridges, the factories; the territorial defence and the militia |

Equipment in outline: T-55 tanks with some T-72s, BMP-1 and BTR-60/70 carriers, D-30 howitzers and rocket launchers,
trucks (the basic models have the T-55, BMP-1, BTR-60, Ural and UAZ). The ground war stays data (decision B4).

## 8. Personnel

### 8.1 Who serves

| Kind | Who | In the game |
|---|---|---|
| **Officers** | From the military schools; lieutenants command platoons and vehicles, captains batteries and fire units, majors and colonels the staffs | Posts in the chain; careers (`docs/air_war/02_AIR_STAFF.md`'s machinery for our side, roadmap 15.2) |
| **Career NCOs and warrant officers** | The technical posts: the senior operators, the radar technicians, the maintenance crews, the battery's sergeant major | Experience that stays when conscripts leave |
| **Conscripts** | Most of the crews, drivers, gunners, plotters and riflemen. In the model countries service was about 18-24 months with call-ups twice a year (research item 8) | Every six months trained men leave and recruits arrive: the battery's skill drops at each intake and has to be trained back up (`06_THE_CREW_AS_PEOPLE.md` §5) |
| **Reservists** | Former conscripts, called up in war | They fill the units kept at cadre strength in peace and replace losses: they know the drill, but are rusty (a tier below their old class) |

### 8.2 Establishments (per unit; estimates unless a source is named)

| Unit | People | Basis |
|---|---|---|
| **Osa battery** | About 45-50 | The PU-12M's battery commander and four numbers (`valka_pu12`, `ru_pu12_text`), four launcher crews of five (6.1), the maintenance crew of four (p189); the MANPADS section, the supply section with the TZM and ATZ crews and the kitchen, and the deputy commanders are estimates |
| **Osa regiment** | About 450-550 | Five batteries, the headquarters battery, the technical battery, the rear (supply, repair, medical, signals): estimate |
| **Regimental AA battery** | About 50 | Four Shilka crews, four Strela-1 crews, the PU-12's crew, ammunition and support: estimate |
| **S-75M2 fire unit** | About 60-90 | Estimate (research item 2) |
| **S-125M1 fire unit** | About 50-70 | Estimate (research item 2) |
| **Radar company** | About 40-60 | Estimate (research item 5) |
| **KS-30 or S-60 battery** | About 50-70 | Guns' crews, the radar and the director, drivers: estimate (research item 4) |
| **Fighter regiment** | Several hundred, about 40-50 pilots | Estimate; with its airfield support battalion beside it (research item 1) |
| **Motor rifle division** | About 9,000-10,000 at war strength | A three-regiment division: estimate (research item 1) |

Totals as orders of magnitude (estimates, to be checked against research item 1): Ground Forces about 60,000-70,000,
National Air Defence about 12,000-15,000, Air Force about 8,000-10,000 in peace, more after mobilisation.

### 8.3 In war

- **Discharges stop** when the war begins. The trained conscripts stay, and the next intake still comes.
- **Mobilisation** fills the cadre units and the territorial defence with reservists, and brings civilian trucks and
  buses into the rear services.
- **Losses are replaced** from the reserve: rusty reservists, or recruits straight from training. Each comes at a lower
  tier than the man they replace (`06` §5).
- **Officers who fall are replaced** by succession, then from the reserve (`08` §3).

## 9. Support equipment and assets

Every unit appears with its vehicles. Each is an object the enemy can see and strike, and a thing the battery's war can
move. Equipment names are families until research confirms the versions.

| Unit | Support equipment and assets |
|---|---|
| **Osa battery** | PU-12M; four 9A33; two TZM 9T217 (modelled); the 9V210M3 maintenance vehicle and the ZIP-2 spares vehicle (p189, p226); the MANPADS section's vehicle; an ATZ (modelled); supply trucks, a field kitchen, the commander's UAZ |
| **Osa regiment** | The headquarters battery's command post, the P-40 and P-19 (in the theatre), the signals vehicles (the R-111 and R-407 are modelled); the technical battery's missile stores and trucks, the 9V242-1 missile test station (6-7 missiles an hour, pp. 188-189) and the 9V914 alignment vehicle; the rear's fuel trucks, workshops, ambulances and kitchens; an engineer platoon's digging machines |
| **S-75M2 fire unit and brigade** | Guidance radar cabins, acquisition radar, height finder, six launchers, transloaders, a diesel power station, cable vehicles; the technical battalion's missile stores, assembly, the fuel and oxidiser tankers and their neutralisation, test stations; the brigade command post's automation (research item 2) |
| **S-125M1 fire unit** | Guidance radar, acquisition radar, four launchers, transloaders, power; the technical battalion's stores and test stations |
| **Kub battery** | Its radar vehicle, launchers, transloaders and power (research item 4) |
| **Regimental AA battery** | A PU-12, four ZSU-23-4, four 9P31 (BRDM-2) launchers, ammunition and missile trucks |
| **S-60 and KS-30 batteries** | Guns, gun-laying radar, director, generator, tractors (heavy ones for the KS-30), ammunition trucks |
| **Radar company** | Two to four radars and a height finder, generators, the plotting post, radios and telephone lines |
| **Sector command post** | The bunker or field post, its plotting, the links to the fighters, the SAM units and the army, its alternate post |
| **Air base** | Aircraft shelters (Kecel's 12 are on the test range's map); fuel bowsers, ground power and starter units, oxygen, munitions trailers and loaders; runway repair (sweepers, rollers, metal planking) and bomb disposal; the airfield's surveillance and landing radars and beacons; the base's ZU-23s and its S-125M1 |
| **Division** | Transport battalion (fuel and ammunition trucks), maintenance battalion, medical battalion, engineer battalion (a pontoon company, bridge layers, digging machines), chemical defence |
| **Army and the nation** | The engineer brigade's pontoon park and the railway troops (the rebuild means); the central depots of fuel, ammunition, missiles and spares; the military transport offices; requisitioned civilian trucks and buses; the railway's rolling stock |

Basic models (`docs/scenery/`): N3 (22 October 2026) made one for every equipment type of the order of battle: the
Strela-1's BRDM-2, the Shilka, the S-60 and the KS-30 with their gun-laying radars and the KS-30's tractor, the
transloaders, the P-18, P-15, P-14 and PRV-11, the propellant tanker, the 9V210M3 and ZIP-2, and our own aircraft in our
colours (the L-39, the Su-22M3, the MiG-23ML, the An-26, the An-2, the Mi-24D, the Mi-8 and Mi-17, the Mi-2). Not yet:
the P-12 and P-37 (no unit of the order of battle has them yet), the airfield vehicles; the MiG-29 only with the later
campaign's option.

## 10. Decisions

| # | Question | Decision or options | Status |
|---|---|---|---|
| 1 | Which S-75 ("S-75SM" in the first request) | **The S-75M2.** SAM Simulator models the Dvina and the S-75M Volkhov (`samsim_game`), so the S-75M2's differences (its missile, its guidance radar's version, its service dates) are research item 2 | **Decided** 22 Oct 2026 |
| 2 | MiG-29s in 1980-85 (first exported 1986-87, to confirm) | **No MiG-29s in 1980-85**; an option in a later campaign's war file. In their place **a squadron of about 12 MiG-23MLs** in the 3rd Fighter Regiment | **Decided** 22 Oct 2026 |
| 3 | The Kub and the two Strela-10 platoons in the theatre | **The Strela-10 platoons become Strela-1 (SA-9) platoons; the Kub battery stays** near Tiszavar | **Decided** 22 Oct 2026 |
| 4 | The Osa regiment's size | **Five batteries (21-25)**, as the textbook (pp. 224-226) and `flak11` have it and the chain already says | **Decided** 22 Oct 2026 |
| 5 | The capital's name and place | **On the Rava, about 60 km north of Kesmark.** The name is yours, to come | **Place decided** 22 Oct 2026; name to come |
| 6 | The army's size | **Two motor rifle divisions of three regiments each (six regimental AA batteries) and a tank brigade; the Kub battery is the tank brigade's air defence** | **Decided** 22 Oct 2026 |
| 7 | One service or two | **Two**: the National Air Defence and the Air Force, beside the Ground Forces (§4.1). Proposed split: the fighters with the National Air Defence, as our chain already works; to confirm (research item 1) | **Decided** 22 Oct 2026 |
| 8 | KS-30 in the 1980s | **Kept** as the capital's static heavy layer | **Decided** 22 Oct 2026 |
| 9 | Where the oil comes from | **Mainly the big Warsaw Pact neighbour's pipeline; a smaller share from the neutral neighbour.** A war with the Pact cuts most of it: fuel shortage and rationing (§3.1) | **Decided** 22 Oct 2026 |

## 11. Research that would source it

| # | What | Where to look first |
|---|---|---|
| 1 | The services' organisation and size in 1980-85 in Czechoslovakia, Hungary, Romania and Yugoslavia: where the fighters sat (decision 7), division and regiment strengths | Operator-country military histories and journals (*Vojenské rozhledy*, already used: `vr_1994_pvov`); `fm100_2_3` as the NATO view to check against |
| 2 | The S-75M2 and S-125M1 fire units: composition, site layout, support vehicles, the technical battalions, crews; how the S-75M2 differs from the S-75M Volkhov that SAM Simulator models | SAM Simulator's manuals and site (`samsim_game`, `samsim_site`); operator documents |
| 3 | The regimental AA battery (Shilka and Strela-1) and the PU-12's use there | `pvo_bu_ix_1984`; `fm100_2_3`; operator documents |
| 4 | S-60 and KS-30 batteries (guns, radars, directors, crews); the Kub battery's composition and its reloaders | Operator documents and manuals |
| 5 | The early-warning radars of the period, their bands, and a radar company's establishment | `p19_to` (in hand); E1's open items |
| 6 | The MiG-23ML's export dates and versions (to confirm a 1980-85 non-aligned buyer); the MiG-29's export dates; the L-39's armed versions; the Su-22 versions | Operator-country aviation histories |
| 7 | Industry and infrastructure of the model countries (power, refineries, pipelines and their sources, rail) | Public statistical yearbooks of the period |
| 8 | Conscription: length of service, intakes, the reserve and mobilisation in the model countries | Operator-country military histories and laws of the period |

## 12. What changes in the game

- **The theatre's data**: from 40 objects to a few hundred, each with its role (`02` §1), defence and rebuild needs; the
  air defences with their radars counted (an S-75M2 or S-125M1 fire unit one radar, an Osa battery four, as the
  balance pass set it, `docs/air_war/09_BALANCE.md`); batteries 23, 24 and 25 added beside 21 and 22.
- **Our order of battle** (`data/command/oob_pannonia.json`, roadmap 13.3) from §4-9: the services, the command posts
  and their alternates, the units, their people and their equipment.
- **Fuel** becomes a national stock with a daily inflow from the two pipelines and the refinery, and a daily burn,
  allocated by priority. A war with the Pact starts with the main pipeline closed. This is the one place where the thin
  model of `02` §3 keeps a national stock, because the brief makes it decisive.
- **Missiles and spares from abroad** stop in a war with the Pact (§3.2); the technical battalions' stocks become what
  the country has.
- **Personnel**: intakes and discharges every six months, reservists in war, replacements a tier below the men they
  replace (roadmap 19.4).
- **The enemy's campaigns** meet a much thicker defence. The balance pass must be re-run (13.8): the rollback, the
  air operation and the small air arms' caution will all change.
- **Performance**: a few hundred objects stays inside the war engine's budget (`05_THE_SYSTEMS.md`).
- **Later playables**: SAM Simulator's other systems (the S-75, the S-125, the Shilka) are on the roadmap's "Later" list.
  This order of battle gives each one a place in the war when it comes.
