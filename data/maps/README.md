# Maps

Generated maps (`simcore/terrain_map.hpp`, `docs/TEST_RANGE.md`): the ground, rivers, roads, railways, towns,
farmsteads, woods, marshes and military sites, built the same way on every machine from a file here. Presentation and
ground for line of sight: nothing here is system data. Positions in km east and north of the map's origin (normally the
theatre's), sizes in metres, headings true; places that exist in `data/air_war/theatre_pannonia.json` or
`data/command/places_pannonia.json` must sit where those files put them (a test checks it).

| File | What |
|---|---|
| `kesmark_range.json` | The Kesmark district as 40 by 40 km, ground out to 150 km |
| `kesmark_rava_dam.json`, `kesmark_dombas.json`, `kesmark_south.json` | The wars' districts (track O, O6): each `"extends"` the Kesmark range (its ground, lines and areas unchanged) and adds one detail area on the far ground at 20 m, its tracks and a village: the Rava dam by Dravec (the Pact), the Dombas hub towards Novo Varosd (NATO), the Lenta front by Lentavar (the neutral). A track that ends on a country road between its bends joins it there in the session's routes (`nav::RoadGraph::ForCountry`) |
| `pannonia.json` | **The whole country** (`"kind": "country"`, `simcore/country_map.hpp`, `docs/COUNTRY_MAP.md`): its ground at 1 km, borders and the neighbour across each, regions, the Rava, Lenta and Tisava, main roads, railways and the two oil pipelines as networks with their river crossings, the capital, cities, towns and the district's villages, forests and landmarks, and its `sites`: with the settlements and landmarks the places everything placed stands at, by id (F1, `docs/fronts/F1_ANCHORS_AS_BUILT.md`: the theatres and the chain's places name them, so moving a site moves what stands there). The Kesmark range is a district of it: its terrain, Rava and main lines are copied here unchanged, and a test proves the two grounds agree |

Change the Kesmark range's terrain, Rava, roads or railways and the country's copy must change with them
(`tests/test_country_map.cpp` fails otherwise). A scenario plays on a map with `"terrain": {"type": "map", "map": "maps/<id>.json"}`. Site kinds: osa_position,
dummy_position, gun_site, fuel_depot, airfield, sam_site, field_post, technical_battery, radar_post, barracks, bunker,
tower, factory, station, power_station.
