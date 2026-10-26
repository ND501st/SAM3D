# The country's map: the whole of Pannonia for the war simulated without a player

The nation on the map, N1 (22 October 2026; `docs/DEVELOPMENT_ROADMAP.md`, "The nation on the map"). The whole country
as a strategic map, `data/maps/pannonia.json`, built by `simcore/country_map.hpp`. It is the map the war runs on when no
player is there: supply runs, marches, the rebuild's logistics and the enemy's interdiction ask it which way goes and
what a cut bridge costs, so whole conflicts can be simulated, tested and developed headless, and the player's battery
is one small part of a large war (`docs/campaign/`).

## What is on it

| Part | What |
|---|---|
| **Ground** | About 330 by 320 km at 1 km: the plain at 110 m, the Kesmark range's relief and hills, and the country's own: the hills on the Pact border, Hradec's hills, the western uplands, the rebels' hills either side of the lower Rava, the eastern rise; and (23 October 2026) **ranges** along a line with peaks and saddles on their crests (the border range against the Pact to over 700 m, the western ridge towards NATO, the Noric ridge, the rebels' hills either side of the lower Rava, Hradec's ridge) and the **gorges** the rivers cut through them (the upper Rava's and the Tisava's through the border range, the lower Rava's through the rebels' hills, the Lenta's), their floors never below the plain |
| **Lakes and marshes** | The Rava reservoir in the gorge above the dam (level 135 m), Lake Siro in the western plain with its reeds; the Tisava's floodplain where it meets the Rava, the lower Lenta's wet meadows. The ground under a lake lies below its level, deepening from the shore |
| **Land cover** | `CountryMap::LandCoverAt`: lake, river, marsh, forest, town or open, for a hide, a column off the road, and the look of the land |
| **Borders** | The country's outline, and who is across each stretch: NATO in the west, the big Warsaw Pact neighbour in the north-east, the Noric Republic in the south-west, a neighbour at peace in the south. Every enemy entry point and base of the theatre lies across its own faction's border; the rebels' lie inside (a civil war) |
| **Regions** | The capital, the industrial belt, the central plain, the northern hills, the western uplands, the south-western border hills, the Rava valley in the south (the briefing's §2) |
| **Rivers** | The Rava from the Pact border past the dam, through the capital and Kesmark and down the rebels' valley; the Lenta past the Lenta bridge to the neutral border; the Tisava past Mezovar and Tiszavar into the Rava |
| **Roads, railways, pipelines** | Networks: every vertex shared by two lines is a junction, every river crossing a named bridge (or a pipeline crossing). Main roads between the capital and the towns and to the border crossings, and secondary roads to the western group and down the Lenta valley to the south-western group and its depot; the railway the Soviet systems' missiles come in by, from the Pact border through the capital; the main oil pipeline from the Pact neighbour and the smaller line from the neutral one, both ending at the refinery. Our supply lines run on them (`docs/air_war/14_LOGISTICS.md`) |
| **Places** | The capital (on the Rava, 59 km north of Kesmark; its name to come), Novo Varosd, twelve towns, the Kesmark district's villages, with populations (game values) |
| **Landmarks** | The refinery, the Rava dam, the two pipeline stations, the border crossings, the capital's river port: the points the theatre's new objects will stand at (N2) |

## How it agrees with what was already built

- **The Kesmark range is a district of it.** Both use one ground function (`NaturalGround`, `simcore/terrain_map.hpp`), and
  the country's file carries the range's base, relief, hills and valley unchanged; the country's own hills are placed
  so none reaches the range. The test compares the two grounds every 500 m across the range: equal to the last bit.
- **The range's Rava and its main roads and railways** are lines of the country, point for point.
- **Every theatre object and every place of our chain** is inside the country; every theatre town is a settlement of
  the country; the theatre's three bridges are crossings of its networks (the Kesmark road and rail bridges over the
  Rava, the Lenta bridge).

`tests/test_country_map.cpp` holds all of it. Change the range's terrain or lines, and the country's copy must change
with them.

## The nation's assets on it (N2)

`data/air_war/theatre_pannonia_nation.json` places the briefing's forces, industry and infrastructure on this map
(`docs/campaign/10_BRIEFING_FORCES_AND_NATION.md` §3-9). It **extends** the theatre the balance pass tuned
(`theatre_pannonia.json`, `"extends"`): the base's 40 objects keep their places in the list, 14 of them replaced where
they stand (the S-75 and the S-125 become the S-75M2 and the S-125M1, the Strela-10 platoons Strela-1 platoons, every
mobile unit gets its prepared positions, and the base's air defences their fire channels), and 130 are added: 170
objects. The radar companies and Osa batteries 23 to 25 are warning posts (`c2_source`). The air war keeps playing the tuned
theatre until N4 moves it here.

| Who | Placed |
|---|---|
| **The National Air Defence** | Its central command post outside the capital; the two sectors' posts, each with an alternate (the 1st's is the bunker on Varos hill); six S-75M2 fire units (three round the capital, Novo Varosd, Paksany, the north-eastern approach) and ten S-125M1 (three in the capital's inner ring, Kecel, Tiszavar, the Kesmark bridges, Paksany, the refinery, Novo Varosd, the dam); the four KS-30 batteries round the capital; ten radar companies along the four borders and east of the capital, beside the Bugac battalion; Bory air base for the 2nd Fighter Regiment |
| **The Air Force** | Soltvar (the Su-22s and the reconnaissance squadron), Lovas (the L-39s), the Dombas helicopter base, the capital's airport (the transports), each with its ZU-23s |
| **The Ground Forces** | The two divisions' six regiments (the four forward groups already in the theatre, two tank regiments' assembly areas), each with its AA battery of a Strela-1 and a Shilka platoon; Osa batteries 23 to 25 and the 15th Regiment's technical battery at the Horka siding; the 9th Division's command post and its S-60 regiment's four batteries; both divisions' artillery; the tank brigade's area and its guns beside its Kub; the army's artillery and engineer brigades (the pontoon park), its rear base and its alternate post; the territorial defence's ZU-23s and Strela-2s at the works, the capital's bridges and the dam |
| **The rear services** | The state's two fuel reserve depots, the central ammunition and missile depots, the S-75M2 and S-125M1 technical battalions, the railway troops' three materials depots |
| **Industry and infrastructure** | The refinery and both pipeline stations at their landmarks; the capital's thermal power station, the dam, five substations; the arms plant, the vehicle works, the radio-electronics plant, the chemical works, the aircraft repair works; the river port; the ministries and the General Staff; state radio and television and the northern transmitter; three exchanges and a radio relay; the capital's marshalling yard; ten more bridges over the Rava and the Tisava; seven more towns |

**On open ground.** Every new object stands inside the country. What stands in the field (SAM sites, radars, air
bases, troops, command posts, depots) is out of the rivers (500 m) and the towns (their radius and 300 m); what needs a
clear horizon or a field (SAM sites, radars, air bases) is out of the forests too and, inside the Kesmark range, whose
own map has the woods, villages and farmsteads, on ground where nothing stands. The refinery, the dam, the pipeline
stations and the port stand at the country's landmarks, every bridge on a crossing of its networks.
`tests/test_country_map.cpp` checks every object and every prepared position.

**Prepared positions.** A theatre object's `"positions"` are the prepared positions it moves between, every
`relocate_h`, never to one within 8 km of the enemy's front (`AirCampaign::Relocate`); without them a unit moves
`relocate_km` in a random direction, as on the tuned theatre, where a long war walks the troops across the country.
Every unit of the nation's theatre that moves on its own has at least two besides where it starts; the player's
battery moves where the player says. A three-day war on the nation's theatre is in the tests: each unit stays on its
prepared positions.

**Our order of battle** (`data/command/oob_pannonia.json`, 97 units) owns every military object once: six S-75M2 and
ten S-125M1 fire units, four KS-30 and four S-60 batteries, five Osa batteries, six regimental AA batteries of a
Strela-1 and a Shilka platoon each, ten radar companies and more, with their equipment and establishments (estimates
unless a basis is named).

## What the simulation can ask it

| Question | Call |
|---|---|
| The ground's height | `GroundHeight(e, n)` (the 1 km grid), `Natural(e, n)` (the analytic ground) |
| Inside the country? Which region? How far to the border, and whose? | `Inside`, `RegionAt`, `DistanceToBorder` |
| The nearest town, the capital, a landmark | `NearestSettlement`, `Capital`, `Landmark` |
| How far to a river, which one; in a forest? | `DistanceToRiver`, `InForest` |
| The way by road, rail or pipeline from here to there, with these bridges down | `Network(net).Route(from, to, cut)`: its length, path, the lines followed and the bridges used |
| The bridges | `Crossings()`, `Crossing(id)`, `NearestCrossing` |

Examples the tests check: by road from Kesmark to Tiszavar is 66 km over the Kesmark road bridge; with that bridge
down it is about 125 km round by the capital; with every road bridge over the Rava down there is no way at all. By rail
the technical battery's siding at Horka reaches the capital only over the Kesmark rail bridge: with it down, the
siding is cut off from the capital by rail. Both oil pipelines reach the refinery; the smaller one crosses the Lenta and
the Rava.

## Running it

```bash
./simcore/build/country_dump data maps/pannonia.json out/pannonia_map.json            # Windows: simcore\build\Release\country_dump.exe
python tools/map_view.py out/pannonia_map.json --png                                  # the country with the nation's theatre: objects, prepared positions, fronts
./simcore/build/country_dump data maps/pannonia.json --route road 9 -1 68 12 b_ralje@rava   # the way round a cut bridge
./simcore/build/country_dump data maps/pannonia.json --height 36 58 85 118          # ground, inside or not, region
```

## The terrain (23 October 2026)

`terrain.ridges` and `terrain.valleys` in a map's file (`MapRidgeDef`, both a map's and a country's): a ridge rises across
its line to its crest (a cosine over its half width) and along it into peaks and saddles (two thirds to four thirds of its
height, by the map's seed); a valley is a trough along a river's line, cutting down through what stands above the plain
by up to its depth, never below the plain's base and 15 m. `lakes` (`MapLakeDef`): a shore, a level and a depth; the
ground under a lake lies below its level, deepening over a kilometre from the shore. `marshes` are polygons, as in a
district's map. `NaturalGround` adds them for both, so the Kesmark range, which has none, is unchanged to the last bit
(the test still compares the two grounds every 500 m across it). In Unreal (`ASimMapActor::BuildCountry`) the country's
ground is tinted by its land cover (reed beds, woods, water) and each lake's surface is a sheet of 250 m cells at its
level; `tools/map_view.py` draws the lakes, marshes and the ranges' crests from `country_dump`'s JSON.

`tests/test_country_map.cpp` ("its terrain"): every range's crest stands high, the border range's gorge has its floor
near the plain (131 m) between shoulders over 700 m, no valley cuts below the plain, the ground under each lake lies below
its level and the land cover there says lake, the reservoir lies above the dam on the Rava, the marshes, a forest, the
capital and the plain say what they are, no asset of the nation's theatre stands in a lake, and the secondary roads reach
the western and south-western groups.

## In the game (N5)

- **The Kesmark range's level draws the whole country around the district** (`ASimMapActor`, `CountryFile`): the
  country's ground at 1 km in place of the map's far ground (the same ground function, so the two meet), the land
  beyond its borders 100 km out at 4 km, its rivers, roads, railways and oil pipelines as ribbons at strategic widths
  (not where the map draws its own), its towns as a patch of town ground with blocks on it (the capital's and the
  cities' taller), the border as a dull red strip; and the nation's assets as basic models (`bDrawAssets`,
  `docs/scenery/README.md`). Every raid of a campaign opens in this level, wherever the battery stands: its ground is
  the map's, whose far ground is the country's. `-SimMapShots` adds six views: the country from 90 km up, the
  refinery, the capital, the reservoir up the Rava's gorge, the border range, Lake Siro.
- **The campaign screen's map is the country's** (`ASimCampaignHUD::DrawMap`): its regions faintly, the border, the
  rivers, railways, main roads and pipelines, its towns (named unless the theatre names them), under the theatre's
  objects, fronts and raids.

## Checks (N6)

`tests/test_country_map.cpp`: the country loads and is the same every time; the Kesmark range inside it shares its
ground, roads and railways; the theatre's objects, prepared positions and our places are inside it and the enemy's entries
and bases outside, each across its own border; the theatre's towns and bridges are the country's; its landmarks have the
theatre's objects of their kind on them and the chain of command's places stand where it has its towns; every asset on
open ground; the loaders refuse what they should (the country, the order of battle, the theatre's `extends` and
`force_scale`, an unknown air defence system); the new kinds parse; the air war on the nation's theatre runs two days
against every adversary, the same every time for a seed. `tests/test_scenery.cpp`: every theatre object has a site to draw.
`air_war_demo_nation` (ctest) and `tools/verify.ps1` / `verify.sh` run the air war on the whole nation against each
adversary with its raids on the Kesmark map's ground.

## What comes next

- Rebuilds routed over the networks (13.5); supply is (`docs/air_war/14_LOGISTICS.md`). Whether the enemy's pre-war
  survey knows a unit's prepared positions is still open.
- Road classes with their speeds for the columns, and the land cover's effect on a column off the road.
- **15.6**: districts generated where the battery fights, from this map's ground (a raid far from Kesmark plays on the
  country's 1 km ground today, plain at close range).

## Known limits

- The ranges and gorges are smooth shapes along lines, at 1 km: no terraces, screes or cliffs, and a narrow gorge or
  the reservoir in it is coarse on the 1 km mesh (the lake's surface is drawn on it at 250 m).
- Lines are straight between their points; the networks know length, not road class speed (yet).
- Each unit has one set of prepared positions whatever the war: the Ground Forces stand where the briefing and the
  theatre's forward groups put them, not deployed towards the front at war (the regiment's tasking, 13.x).
- The Kesmark range's own far ground (out to 150 km, `kesmark_range.json`) is still its own, not this map's: a raid
  played on the range sees the range's far ground. Drawing the country beyond the range is N5.
