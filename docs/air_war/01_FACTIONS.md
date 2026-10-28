# 1. The four adversaries and the theatre

Data: `data/air_war/faction_<id>.json` (loader `simcore/air_war/data.hpp`, `LoadFactionFile`); the theatre
`data/air_war/theatre_pannonia.json`. Research: `docs/research/ENEMY_AIR_RESEARCH_2026-10.md` §2.

Every faction file has the same blocks, each with `source` and `confidence`:

| Block | What |
|---|---|
| `doctrine` | planning cycle and lead time; on-call share; centralisation (punctual and rigid vs ragged and flexible); simultaneous vs successive bias; sortie rate, serviceability, packages per cycle; **phases** (apportionment of sorties to the nine mission kinds, until a day or until the believed air defences fall below a share); **package templates** per mission kind (main flights and size, preferred aircraft, SEAD per believed radar threat, escort ratio, jammers stand-off or escort, decoys, chaff, additional reconnaissance, allowed heights and times of day) |
| `intelligence` | satellite passes and latency; reconnaissance film latency; ELINT reach, delay and accuracy (`elint_best_m + elint_k_m / sqrt(seconds)`); radio intercept; agents (rate, delay, error, false share, reach); pre-war knowledge of mobile units; pilots' over-claiming; analyst skill |
| `logistics` | replacements per day; ARM stock and resupply; damaged aircraft return time |
| `politics` | support and pressure; loss tolerance per day; sensitivity to prisoners and civilian casualties; no-strike radius around towns; depth limit; banned target kinds; interference from above per cycle; the faction's worth of each target kind; **events** (conditions → bans lifted or added, pauses, tempo, a height floor, reinforcements, ARM resupply, depth and no-strike changes) |
| `aircraft` | types with roles (strike, dead, sead, hunter, escort, jammer, recce, decoy, helicopter), the gameplay hit-chance class, RCS, speeds, low and medium heights, turn rate, night and all-weather, EM RWR and jammers, ARMs (count, range, memory), strike power, recce quality, air-to-air, evasion. Every number is a PLACEHOLDER |
| `squadrons` | id, name, aircraft, count, leader (an officer id or generated) |
| `officer_pool`, `officers` | rank titles by role, name pools, backgrounds, trait and skill means; the authored staff (`02_AIR_STAFF.md`) |
| `em_libraries`, `callsigns` | the EM libraries its jammers and RWRs come from; flight call signs |

## Inferior: the Rava Liberation Front (`faction_rebels.json`)

The shape of Croatia's improvised air arm of 1991 (`croatia_1991_migflug`): four An-2 crop sprayers with home-made bombs,
three light aircraft, two Mi-8s with door guns, two defected light jets. One phase, *harassment, supply and propaganda*:
night harassment, helicopter raids, small strikes, reconnaissance by a pilot's eyes. No SEAD, no DEAD, no jammers. Agents
everywhere (reach 1000 km), radio intercept on captured sets, nothing else; claims inflated twofold. Loss tolerance 0.3
aircraft a day; targets chosen for effect (headquarters, troops), its own towns banned; raids no deeper than 80 km. Its
commander flies the night raids himself and keeps a list.

## Near peer: the Noric Republic (`faction_neutral.json`)

The scale of Austria's air force (`austrian_af_wiki`), fictional: 26 light jets, 10 armed utility helicopters, 12
gun-armed interceptors, 4 observation helicopters. Phases: *defence of the border* (three days: close support, helicopter
raids, reconnaissance), then *limited raids along the front*. A border ELINT station (40 km reach), observers, camera
pods. Loss tolerance one aircraft a day; towns, power stations, factories and headquarters banned; nothing deeper than
40 km (an event can lift it to 60). A cautious, loss-shy commander and a planner who flies his raids like a display at
first light.

## Peer: the Warsaw Pact (`faction_warsaw_pact.json`)

The Soviet-lineage method of `mai_tactics_vvs` with the organisation of `fm100_2_3` and the air operation of
`cia_1985_air_op`: a 12-hour cycle planned at army level, punctual (centralisation 0.9) and massed (simultaneous bias 0.8).
Phase 1, *the air operation* (two days: airfields, command posts, radars, with suppression and demonstration groups);
phase 2, *support of the front*. Strike packages are tactical groups: a strike group, a MiG-25BM suppression group with
anti-radiation missiles, a MiG-23 air-clearing and escort group, a Yak-28PP standing off, additional reconnaissance ahead.
Hunting flights where a battery is suspected; demonstration groups of real aircraft. Satellites with 36-hour film
latency, Il-20-class ELINT (150 km), agents. Loss tolerance six a day; a political officer on the staff; the front
commander demands maximum efforts.

## Superior: NATO (`faction_nato.json`)

A composite allied tactical air force: a 24-hour air tasking order planned 12 hours ahead (`usaf_ato_doctrine`). Phase
1, *air superiority and the rollback of the air defences*, until the intelligence believes fewer than 45% of them work:
DEAD with decoys first, hunter-killer patrols, decoy waves, strikes on the warning radars and command posts; then
*interdiction*, then *support of the land battle*. Strike packages carry F-4G/F-4E hunter-killer SEAD, an EF-111A standing
off with ALQ-99-class jamming in the Osa's search band, escorts, chaff (`afmag_1989_slam`, `jp3_01_4_1995`). Satellites
twice a day, TDOA ELINT that locates a new emitter within seconds (`vr_1985_plss`), radio intercept, agents. 60 ARMs and
three a day (HARM was in short supply). Loss tolerance four a day, very sensitive to prisoners and civilian casualties,
a 4 km no-strike zone around towns, power stations banned until the alliance escalates.

## The theatre (`theatre_pannonia.json`)

The Pannonian Socialist Federal Republic (fictional; rename freely) on the plain at the existing scenarios' origin. Four
fronts, one per adversary, each with its entry points, the enemy's bases, the defender's fighter threat and where the
player's battery deploys in a war on that front. Thirty-six objects: the player's Osa battery 21 and its two dummies, the
Kesmark bridges it defends, guns, depots, air bases, an S-125, a Kub, an S-75, a neighbouring Osa battery, Strela-10
platoons, the regiment's P-19 and P-40 (they feed the PU-12's picture), the radio-technical battalion (the division's
warning net), command posts and headquarters, a power station, a rail junction, a factory, the forward troops of four
groups, six towns. Each has a value, hardness, repair time, mobility (and how often and how far it moves), emission,
pre-war knowledge and, for air defences, a defence (system, radius, lethality, low and medium cover, radar or optical,
rounds, the fire-control radars an ARM can take one by one, and on the nation's theatre the fire channels). All game
values.

**The war's year** (`"year"`, 1985): an aircraft or missile type with a later `in_service_year` doesn't fly, so the
MiG-29 of a later campaign and NATO's cruise missiles (the Tomahawk 1986, the CALCM 1988) wait for a later war; the
Pact's Kh-22s from its Tu-22M bombers fly in this one (`09_BALANCE.md` §8).

**The whole nation** (`theatre_pannonia_nation.json`, the nation on the map N2, `docs/COUNTRY_MAP.md`) extends this
theatre with the briefing's forces, industry and infrastructure, 170 objects on the country's map, and gives every
mobile unit its prepared positions (`"positions"`), which it moves between. The campaigns play this theatre until N4
moves them there.

Planned in the battery's war (`docs/campaign/`): each faction's war aims in its file
(`docs/campaign/01_WAR_AIMS_AND_TARGETS.md` §2), its reach over our country read as game logic, and our doctrine
against it (`docs/campaign/08_COMMAND_DOCTRINE_AND_REACH.md` §1-2).
