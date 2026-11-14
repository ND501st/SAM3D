# 14. Our supply lines and the enemy's armed reconnaissance

Written 23 October 2026. The player's side's transport and supply: convoys, trains and armoured march columns going
round between depots and troops, on the country map's roads and railways, and the enemy hunting them. Code:
`simcore/include/simcore/air_war/logistics.hpp` (`SupplyNetwork`), the theatre loader's `supply_lines` and `MakeColumns`
(`data.hpp`), the planner's armed reconnaissance (`planner.hpp`), the search in `resolve.hpp` (`FindColumn`); data: each
theatre's `supply_lines`, the `logistics` group of `data/gameplay/air_war.json`. Research:
`docs/research/AIR_WAR_MISSIONS_2026-10.md` §6.

## The lines

A supply line (`TheatreDef::supply_lines`) runs from a depot, rail junction or port to the troops it feeds, by road or
rail, through any objects named on the way (a bridge). Its columns are theatre objects the loader makes (`<line>_1`,
`<line>_2`...): kind `convoy` (trucks), `train` (wagons, the men, tanks on flatcars) or `column` (an armoured march
column with its own ZSU-23-4 platoon), mobile, each at its depot at the start, its speed known to the enemy's picture.

| Theatre | Lines |
|---|---|
| Tuned (`theatre_pannonia.json`) | the southern and northern groups' supply roads (the northern over the Kesmark bridge), the western group's railhead line from Cegled, the border battalion's road, the northern group's march replacements (at night only) |
| The nation (`theatre_pannonia_nation.json`) | those, and the fuel road from the state reserve, the tank regiments' and the artillery's ammunition roads, the western road from the central ammunition depot, the southern trunk railway from the capital over the Horka rail bridge, the south-western group's fuel road: 14 lines, 52 columns |

A convoy is 18-22 trucks (the materiel support battalion's companies of 60 and 80: `fm100_2_3` p. 4-90), a train 32-36
wagons, a march column some 30 vehicles. An intact line brings half again what its troops use: `delivery` per column is
set from the round's length (game values).

## A round

Each hour (`SupplyNetwork::Step`): a column loads at its depot (`load_h`), drives or runs its line at `speed_kmh`,
unloads at the troops (half the loading time) and comes back. Columns leave spread round their round. A night-only line
moves in the dark (night and twilight) only. What a convoy or train brings tops up the troops' stocks; a march column
brings strength. Troops a line feeds use a third of their stocks a day (`logistics.consumption_per_day`); once out they
lose `unsupplied_decay_per_day` (0.15) of their strength a day down to `unsupplied_floor` (0.25), and below
`repair_needs_supply` (0.2) they mend nothing.

## The roads and the bridges

On the nation's theatre a line follows the country map's network (`CountryNetwork::Route`), the last stretches at each
end on local roads (`logistics.snap_m`, 30 km; a place off the network is reached straight). A bridge object stands on a
crossing of the map; below `logistics.bridge_open` (0.7: a span down, the deck holed) it carries no traffic. When one
goes down or comes back every line takes its way again: round by another crossing (the journal: "goes round by another
crossing (64 km instead of 37 km)"), or cut, its columns stopping where they are ("is cut: no way round a bridge that is
down"), until the bridge is mended ("is open again"). On the tuned theatre a line runs straight through its waypoints and
a bridge within a kilometre of it is on it. A destroyed column is replaced from the reserve after `replace_h` and starts
again at its depot. The ledger (`SupplyLedger`) counts what was delivered and lost, the vehicles and columns destroyed,
the line-hours cut and the reroutes.

## Armed reconnaissance

`MissionKind::ArmedRecce`: NATO's follow-on forces attack (`wiki_fofa`) and the Soviet free hunt. Its candidates are the
enemy's sightings of our columns, one a line (the freshest); whatever the sighting's age, the search flies to the point of
the line nearest the front (the roads are on every map; the columns' times are not). The planner's chance of a find is
its rule of thumb (`SupplyNetwork::OnStretchChance`): how much of its round a column spends on the road, how much of the
road the stretch covers (`logistics.search_m`, 15 km each way), how many columns run, nothing by day on a night-only
line, times the chance to see one there (`ColumnFindChance`: 0.7 by day, 0.5 at dawn or dusk, 0.15 at night, 0.4 with a
night attack system, half under low cloud). It is judged by what it does when it finds a column, not by the find's
chance. In the resolver the flight searches its stretch and attacks the nearest column it sees on the move (one loading
or unloading is inside its depot or with its troops); found, the column is photographed (a sighting for the enemy's
picture); nothing found, the flight brings its weapons home. Columns carry their own air defence where they have it.

| Faction | Template | Share of the cycle |
|---|---|---|
| NATO | pairs of A-10s, F-16s, F-4Es, low or medium, by day; SEAD and escort as needed | 3 % in the rollback, 8 % in interdiction, 6 % supporting the land battle |
| Warsaw Pact | pairs of Su-25s, MiG-27s, Su-17M3s, low, by day | 3 % in the air operation, 6 % supporting the front |
| Neutral | pairs of Saab 105s | 5 % in its limited raids |

The doctrines weigh the columns (`target_weights`): NATO 1.5, the Pact 1.3, the neutral 1.1.

## What it gives

Over a week (`09_BALANCE.md` §10): the Pact flies 11-16 searches and NATO 6-8, a column found in a quarter to a half of
them, some 20-40 vehicles destroyed; bridges down cut the lines for 25-85 line-hours and send them round once or twice;
the troops end the week with 81-95 % of their stocks. The supply war is the enemy's slow grind, not a decisive blow in a
week; it bites when bridges stay down.

## Tests

`tests/test_logistics.cpp`: the lines load and their columns are objects at their depots; the loader refuses an unknown
depot, a line to anything but troops, a train by road, an unknown column kind, a speed out of range; the columns go
round and keep the troops supplied, a bridge down cuts the northern road and its columns stop, the northern group's
stocks run out and it weakens, mended it opens; on the nation's map 12 of the 14 lines follow the networks, each bridge
down sends lines round or cuts them, the southern railway is cut at Horka; an armed reconnaissance finds a column on the
road about as often as its find chance by day and by night and never one in its depot, a column's position spreading in
the enemy's picture at its speed; NATO and the Pact search our lines in two campaigns each and find columns.

## Open

- A column's speed by road class and by night; the march's discipline under air attack.
- The engineers' pontoon bridges (the pontoon park) shortening a cut line's wait.
- What our side's people report of the supply war, through the chain of command (`docs/command/`): today it is in the
  journal only, not yet a fact that reaches the player's character.
