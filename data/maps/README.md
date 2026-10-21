# Maps

Generated maps (`simcore/terrain_map.hpp`, `docs/TEST_RANGE.md`): the ground, rivers, roads, railways, towns,
farmsteads, woods, marshes and military sites, built the same way on every machine from a file here. Presentation and
ground for line of sight: nothing here is system data. Positions in km east and north of the map's origin (normally the
theatre's), sizes in metres, headings true; places that exist in `data/air_war/theatre_pannonia.json` or
`data/command/places_pannonia.json` must sit where those files put them (a test checks it).

| File | What |
|---|---|
| `kesmark_range.json` | The Kesmark district as 40 by 40 km, ground out to 150 km |

A scenario plays on a map with `"terrain": {"type": "map", "map": "maps/<id>.json"}`. Site kinds: osa_position,
dummy_position, gun_site, fuel_depot, airfield, sam_site, field_post, technical_battery, radar_post, barracks, bunker,
tower, factory, station, power_station.
