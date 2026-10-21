# The chain of command and its signals, 1980-85: research for the information pipeline

Written 18 October 2026, after the project owner's direction: SAM3D is first person; intelligence, early warning and
after-action reports must reach the player's character through people and means of communication, not HUD panels; the
NPCs need a realistic chain of command, senior officers included, present on a map of a small city, towns, villages,
industrial sites and countryside. The design built on this is `docs/command/`.

Sources follow `docs/SOURCES.md` (tiers); new ones are registered there. Where the period's own documents could not be
read, a later document of the same (Soviet) lineage is used and says so.

## 1. Sources read

| Key | Tier | What it is | Used for |
|---|---|---|---|
| `utin_2018` | 1 (later than the period, Soviet lineage) | L. L. Utin, R. A. Bozhko, S. V. Rusak, *Основы организации связи* (Fundamentals of organising signals), BSUIR military faculty, Minsk 2018; approved by the Belarusian Ministry of Defence as a cadets' textbook | Kinds of command post; the means of signal; mobile (courier and postal) signals and their three schemes; precedence categories and connection-time limits; signals in a city |
| `fm100_2_3` | 1 for what NATO believed (registered) | US Army FM 100-2-3, *The Soviet Army: Troops, Organization and Equipment* (1991) | The SAM regiment's organisation (SA-6 chart p. 4-62; the SA-8 regiment p. 4-101); the regimental headquarters' signal equipment, its warning receivers and despatch motorcycles; the division signal battalion's courier platoon |
| `uvs_rf_2007` | 1 (later than the period, Soviet lineage) | *Устав внутренней службы Вооружённых Сил Российской Федерации* (the internal service regulations), decree no. 1495 of 10 November 2007; read from a search excerpt of art. 96 | The regiment's officials: the chief of staff is the commander's first deputy and the direct superior of all the regiment's personnel |
| `flak11` | 1 (registered) | Veterans of NVA Fla-Raketen-Regiment 11 (Osa-AK) | The regiment of five firing batteries, a headquarters battery with the PU-12M and the radars, a technical battery and a transport company (already in `OSA_RESEARCH_2026-10.md` §7) |
| `basta_lm` | 1 (historical study, operator country), read from a search excerpt | J. Bašta, dissertation on the Lidové milice (the Czechoslovak People's Militia), Charles University, Prague | The party's armed militia guarded industrial enterprises, helped the police patrol, and in war had combat tasks on the state's territory |
| `valka_lm` | 4, read from a search excerpt | valka.cz, *Lidové milice [1970-1989]* | The militia's staffs from squad and platoon to battalion, district, region and the nation |

Searched without a usable result: the Soviet 1975 internal service regulations in full; FM 100-2-1 (the 1984 *Operations
and Tactics*: the copy could not be read as text); the NVA's own regiment pages (the veterans' site and the web archive
could not be reached); Hungarian sources on the Workers' Guard's wartime role. These are open items (§7).

## 2. Command posts and who is at them

`utin_2018` §1: a commander controls from **command posts** (пункты управления), fixed or mobile:

| Post | What it is |
|---|---|
| Command post (КП) | The main post: the command group, the chiefs of arms, the signal centre, the support group |
| Forward post (ППУ) | Where the commander goes to the troops, or controls a unit on a most important task |
| Airborne post (ВзПУ) | On helicopters, to control the battle on the move |
| Auxiliary post (ВПУ) | For a group on a separate axis |
| Alternate post (ЗКП) | Takes over if the KP is lost; **usually headed by a deputy commander** |
| Rear post (ТПУ) | Controls the rear and technical support; takes over temporarily if both KP and ZKP are lost |

The staff splits into a commander's group and a chief of staff's group that move with them, and officers fixed to the
KP and the ZKP. In SAM3D: the regiment has a KP (the commander, the chief of staff, the duty officer, the chief of
signals, the PU-12M), a rear post (the deputy for the rear, the transport company, the field kitchen, the medical post),
and the technical battery under the deputy for armament; the division's air defence command post is the regiment's
superior; the battery's PU-12M is the battery's post.

## 3. The regiment and its officials

- **Organisation** (`fm100_2_3` p. 4-62 for the SA-6 regiment, the SA-8 regiment being the alternative; `flak11` for the
  NVA's Osa-AK regiment): a regimental headquarters (about 60 men), a target-acquisition and early-warning battery (55;
  in the NVA's Osa regiment the headquarters battery holds the PU-12M, a P-40, two P-19 and a P-18), five firing
  batteries, a missile technical battery (85), a motor transport company (80), a maintenance company (50) and a chemical
  protection platoon (24).
- **Officials** (`uvs_rf_2007`, positions of the Soviet regulations kept in the Russian ones): the regiment commander;
  the chief of staff, first deputy and direct superior of every man in the regiment; the deputies for armament and for
  the rear; in the 1980s the deputy for political affairs (the *zampolit*; the NVA's *Stellvertreter für politische
  Arbeit*); the chiefs of signals and of reconnaissance; the duty officer of the command post. The battery commander
  answers to the regiment commander; in battle his battery covers a motorised rifle regiment or an object (here the
  Kesmark bridges), whose chief of air defence he works with.
- **The regiment's signals** (`fm100_2_3`, the regimental headquarters' signal company of an SA-6 regiment): HF and VHF
  radios (R-104M, R-130, R-118, R-107, R-123), radio relay (R-401/405), three R-311 **warning receivers** (the division's
  early-warning net), the TA-57 field telephone and the P-193M switchboard, and **three motorcycles that give the
  regimental commander messenger and courier service**. The division's signal battalion has a telephone and telegraph
  company with a **courier platoon** (13 motorcycles).

## 4. The means of signal

`utin_2018` §2, §6: the technical means of control are **electrical** (radio, radio relay, tropospheric, satellite, wire),
**mobile** (transport) and **signal** means.

- **Mobile means** (§6.6) carry documents: the **courier service** (фельдъегерская связь: all combat and service documents,
  orders, instructions, reports) and the **postal service** (letters, parcels, the press). Unlike electrical means they
  deliver "a practically unlimited quantity of documents in their original form", which cannot be distorted or
  overheard. Planning takes the terrain, the state and load of the routes, where the posts and landing sites are, and
  the couriers' guard into account. Three schemes:
  - a **direction** between two posts along a fixed route: the fastest, for the units with the most important tasks,
    but costly in vehicles and men;
  - a **circular route** from a senior post through two or more subordinates in one run, one after another: fewer
    vehicles, longer delivery; the common scheme in formations, units and the rear;
  - an **axis** through an **exchange point** the senior staff sets up in the subordinates' area: for groups on separate
    axes or far away.
  Couriers (фельдъегеря, посыльные) and military postmen carry them; mobile means have **priority on every road and
  crossing**, a recognition sign, and the couriers an identity document.
- **Choosing the means** (§5): radio is the main and often the only means with units on the move; "under active enemy
  radio reconnaissance and strong jamming, wire and mobile means are more covert", and the means chosen should hide not
  only the content but the fact of transmission. **This is the information pipeline's central trade-off for the player**:
  a radio call is fast and tells the enemy's intercept service something; a despatch rider is slow, safe from intercept
  and at risk on bombed roads.
- **Timeliness** (§3.1): a message's value falls with time to zero; the measure is the time a message spends in the
  system (for documents) or waits for a connection (telephone). Priority is by **precedence**: telegrams "Гранит",
  "Воздух", "Ракета", "Самолёт", "Обыкновенная"; telephone passwords "Гранит", "Воздух", "Самолёт", "Связь-авария";
  categories "out of all turn", "first", "second", "general". Table 3.1 limits the time to provide a "Гранит" call to
  1 minute direct, 2, 3 or 5 minutes through one, two or three transit centres (the other rows did not survive the text
  extraction and are not used). Calls are limited to 2 minutes ("Гранит"), 5 ("Воздух"), 10 ("Самолёт"); "Гранит" and
  "Воздух" telegrams to 25 groups, the others to 300. A telegram's time = the auxiliary operations at both ends (taking
  it in, carrying it to the equipment, handing it over) + waiting + transmission. Timeliness "matters most in the
  missile troops and the air defence troops": the air warning is "Воздух".
- **In a city** (§9.3): radio range falls (VHF most) among buildings; wire lines are hard to lay and often broken by
  rubble and fires; mobile and signal means are limited; signal centres go into cellars and tunnels; couriers may move
  underground; the city's own telephone network may be used, with measures against eavesdropping.

## 5. The territory: who else is in the town and the villages

The player's country resembles Czechoslovakia, Hungary, Romania and Yugoslavia. Each had, besides the army:

- a **garrison and its commandant** (the Soviet-model *военный комендант гарнизона*: discipline on the streets, patrols,
  the guard of garrison objects; in war traffic control and the movement of troops through the town);
- **civil defence** headquarters in every district and town (sirens, shelters, rescue, damage reports);
- the **police** (in Czechoslovakia the Veřejná bezpečnost, in Hungary the Rendőrség);
- the party's **armed militia** in the factories (Czechoslovakia's Lidové milice: guarding enterprises, with combat tasks
  on the state's territory in war, `basta_lm`; staffs from the platoon to the district, `valka_lm`; Hungary's
  Munkásőrség, Romania's Gărzile Patriotice, Yugoslavia's Territorial Defence played the same part);
- the **national air defence** (fighter regiments, the country's SAM units, the radio-technical troops), whose sector
  command post gives the army's air defence its strategic warning, and the fighters whose flights the batteries must
  know of;
- local government: the district and the village **national committees** (councils), the railway station's military
  transport office, the hospital, the post office.

These are the people who see a parachute come down over a village, hold a captured airman until the army collects him,
report a bridge down, sound the sirens and carry a rumour. Only the first three lines of this section are sourced to
the period's own documents in this note; the rest is the design's reading of how those states were organised, marked
as such in the data (`confidence`: "estimate").

## 6. What this changes in the design

1. **Nothing reaches the player's character except through a person or an instrument.** The air war's intelligence
   summary, its raid warnings, the prisoners' interrogations and the after-action results are facts held by a position
   in the chain (the national sector's duty officer, the division's intelligence officer, the regiment's chief of
   staff) and travel down it by the means each pair has, at each means' speed, with its losses.
2. **Two rhythms.** Urgent facts go at once by the fastest means the precedence allows (the "Воздух" warning by radio
   and the warning receivers, an order by telephone); routine ones wait for the next **contact**: the evening briefing
   at the regiment's KP, the commander's or the political deputy's visit, the courier's circular run, the post.
3. **The means are a choice with a cost.** Radio is fast and heard by the enemy's intercept service (the air war's
   COMINT); a despatch rider is covert and slow, and the road may be cut; the telephone needs the wire to be whole.
4. **Senior officers are on the map**: the army's chief of air defence in the city; the division's in its command post
   in the woods near a village; the regiment's officers at its KP and rear; the garrison commandant, the civil defence
   chief and the police in the town; the militia at the works; the national sector's command post in a bunker on the
   city's edge; the fighter regiment at the air base.

## 7. Open items and documents to acquire

- The Soviet internal service regulations of 1975 (the officials of a regiment in the period's own words) and FM 100-2-1
  (1984) chapter on command and control (as NATO saw it).
- The NVA's *Handbuch für Fla-Einheiten* (`nva_handbooks`) for the Osa regiment's command post and its nets.
- Telephone connection limits for "Воздух" and the other categories (table 3.1 of `utin_2018`, from a clean copy).
- The despatch riders' speeds and the courier runs' schedules in the period's norms (today estimates).
- Czech, Hungarian and Romanian sources on civil defence, the militias and the garrison commandants in war.
