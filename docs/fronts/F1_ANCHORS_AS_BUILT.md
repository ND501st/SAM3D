# F1, anchors: the map as the source of truth (as built, 20 Nov 2026)

The project owner, 19 Nov 2026: "If I shift a factory by 2 km, the source of truth is updated automatically." Roadmap
track F, row F1 (`docs/DEVELOPMENT_ROADMAP.md`). Every placed object now takes its position from the country map by id.

## What it is

- **Places.** A place of the country map (`data/maps/pannonia.json`, `simcore/country_map.hpp`) is a settlement's
  centre, a landmark or a **site**, one namespace: the loader refuses an id in two lists. Sites are the new list: a
  point with an id, a name, `east_km` and `north_km`, and for a bridge the crossing it stands on (`"crossing"`). They
  are game layout, not system data.
- **Anchors.** A file on a place map names it (`"place_map": "maps/pannonia.json"`, relative to the data folder) and
  gives each point as `"at": "<place>"` instead of coordinates:
  - the theatres (`data/air_war/theatre_pannonia.json`, `theatre_pannonia_nation.json`): every object, each of its
    prepared positions (`"positions": [{"at": ...}]`), each front's `bases`, `battery_position` and `missile_launch`;
  - our chain's places (`data/command/places_pannonia.json`).

  Depots and the supply lines' ends were already objects by id (`from`, `to`, `via`), and the columns start at their
  depot, so they follow it. Our order of battle (`oob_pannonia.json`), the chain (`chain_pannonia.json`) and our
  aviation (`own_air.json`) already named objects and places by id.
- **Refusal.** A file on a place map that names a place the map lacks is refused, naming the path and the place
  (`objects[3].at: no place 'x' on the map maps/pannonia.json (add it to its sites)`), and so is a coordinate where a
  place belongs. A file without `place_map` keeps its coordinates (the tests' inline theatres) and is refused if it
  names a place. Loaders return false with the message: no exceptions.
- **Moving a place moves everything anchored to it**, with no other edit: change a site's `east_km` in the map, and the
  objects, prepared positions, front points and chain places that name it, and the columns at a depot that does, move
  with it on the next load.

## Cheap at load, nothing at run time

`MapPlaces` (`simcore/country_map.hpp`) is a hash map from id to point, built once when a file on a place map loads
(`LoadMapPlaces` reads only the map's three lists, not its ground, so no grid is baked for it). The loaders resolve each
id to its point there and keep the point (`TheatreObject::pos`, `prepared`, `FrontDef::bases`, `command::Place::pos`):
the war never looks a place up by name. An extending theatre on the same map as its base reads the map's places once.

## Bit-identical

The migration, `tools/anchor_places.py`, moved 349 points into 299 new sites. A point at a settlement's or a landmark's
exact point takes that place; points at the same exact point share one site (the district's and the nation's copies of
an object; a chain place at its theatre object, e.g. `nv_works` at `csepely_works`); any other gets its own. A site keeps
the old number's text, and both loaders compute `km * 1000.0`, so every position is the same double as before. Proven
three ways:

- the script refuses to write unless every point, read through the new map, equals the old `km * 1000.0` (and its
  pytest, `tools/tests/test_anchor_places.py`, checks the real data against the commit before the migration);
- every position the loaders produce (both theatres' objects, prepared positions and front points, the chain's places)
  dumped as hex doubles before and after: identical;
- `air_war_balance data 20 7`, `air_war_balance --theatre theatre_pannonia_nation.json data 10 7` and the replay
  fingerprint (`tests/test_routes.cpp`: journal 107, FNV 1375944258174313379, 41 packages): byte-identical.

## Choices

- **A separate key, `place_map`, not `country_map`.** The nation's theatre names a `country_map` and routes its supply
  lines on its roads; giving the district theatre one would have changed its balance. `place_map` only says where
  points come from.
- **No offsets.** A place is a point, not a point relative to a town: an office in Novo Varosd has its own site, and
  moving the town's centre does not move it. This keeps the migration exact; a site relative to a settlement can come
  later if the owner wants towns to carry their buildings.
- **Bridges get their own sites.** The map's crossings are computed from where its roads cross its rivers; anchoring a
  bridge to a crossing would have moved it. Its site names the crossing instead, and `tests/test_anchors.cpp` checks
  it is the crossing the supply network finds under the bridge (`air_war/logistics.hpp`, within 800 m): move a road and
  the test says which bridge to move.
- **A front's line and entry points stay coordinates.** They are the fronts' geometry, which F2 (the air front by
  sector) and F3 (the line drawn from the units) replace; they are not placed objects.

## Tests

`simcore/tests/test_anchors.cpp`: the places are one namespace and every anchored point stands on its place to the bit;
four places moved (a factory 2 km east, the town of Kesmark 2 km south, a prepared position, NATO's air bases): what
names each moved by exactly that, and nothing else moved a bit; the refusals (a missing place for an object, a prepared
position, a front's bases and a chain place; coordinates on a place map; a place without one; a theatre on a place map
loaded without its places; an id twice on the map; a district map as a place map); the bridges' crossings.
`tools/tests/test_anchor_places.py` and `tools/tests/test_map_view.py` (the map view reads the points through the
places).

## Left

- **The enemy's listeners** (`data/command/intercept.json` `enemy_listeners`) keep their own coordinates: the enemy's
  receivers beyond the border, not on our map's list of placed things in F1's row.
- **The Kesmark range** (`data/maps/kesmark_range.json`) is a district map with its own sites, woods and bridges: its
  battery positions and bridges are not tied to the country's sites, so moving a site the district also draws (the
  Kesmark bridges, battery 21's position) needs the district's edit too.
- Scenarios (`data/scenarios/`) place their units in their own frame; a scenario's `"nav"` already asks routes between
  anchors by id (`simcore/nav/`).
