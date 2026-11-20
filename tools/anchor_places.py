"""Anchor the theatre's and our chain's places on the country map (roadmap F1, docs/fronts/F1_ANCHORS_AS_BUILT.md).

Every placed object takes its position from the country map by id: the map is the only source of truth for where things
are. This is the one-off migration that moved the coordinates out of the theatre files and the chain's places file into
the map's "sites" list:

  * data/air_war/theatre_pannonia.json and theatre_pannonia_nation.json: an object's east_km/north_km becomes
    "at": <place>; each prepared position, each point of a front's line and each front's bases, battery_position and
    missile_launch becomes {"at": <place>}, each of its entry points {"id": ..., "at": <place>};
  * data/command/places_pannonia.json: each place's east_km/north_km becomes "at": <place>;
  * data/maps/pannonia.json: a new "sites" list after its landmarks; each file above gets "place_map".

A place is a settlement's centre, a landmark or a site, one namespace. A position at a settlement's or a landmark's exact
point takes that place; positions at the same exact point share one site; any other gets its own site, named after its
object (an object: its id; a prepared position: "<object>_pos<n>"; a front's point: "<faction>_<key>", its line's
"<faction>_line<n>", its entries' "<faction>_entry_<id>"; a chain place: its id; "_site" added where a place of the map
already has that id). A site keeps the old number's text, so the
loader's km * 1000.0 is the same double as before, bit for bit. With --crossings (country_dump's JSON), a bridge
object's site names the crossing simcore's supply network links it to.

    python tools/anchor_places.py [--data data] [--crossings out/pannonia_map.json] [--dry-run]

Run on data already migrated it finds nothing to move and changes nothing; run on data partly migrated it anchors the
rest, adding its sites after the map's.
"""
import argparse
import json
import math
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

MAP = "maps/pannonia.json"
THEATRES = ["air_war/theatre_pannonia.json", "air_war/theatre_pannonia_nation.json"]
PLACES = "command/places_pannonia.json"
FRONT_POINTS = ("bases", "battery_position", "missile_launch")
BRIDGE_LINK_M = 800.0  # simcore/air_war/logistics.hpp: a bridge object stands on the crossing nearest it within 800 m

NUM = r"(-?[0-9][0-9.eE+-]*)"
PAIR = re.compile(r'"east_km":\s*' + NUM + r',\s*"north_km":\s*' + NUM)


def blocks(doc):
    """Every dict carrying east_km, with its path, in the order its east_km appears in the file's text."""
    out = []

    def walk(v, path):
        if isinstance(v, dict):
            for k, x in v.items():
                if k == "east_km":
                    out.append((path, v))
                walk(x, path + (k,))
        elif isinstance(v, list):
            for i, x in enumerate(v):
                walk(x, path + (i,))

    walk(doc, ())
    return out


def role(kind, path):
    """What a coordinate block is: 'object', 'position', 'front' or 'place' (anchored), or None (kept)."""
    if kind == "theatre":
        if len(path) == 2 and path[0] == "objects":
            return "object"
        if len(path) == 4 and path[0] == "objects" and path[2] == "positions":
            return "position"
        if len(path) == 3 and path[0] == "fronts" and path[2] in FRONT_POINTS:
            return "front"
        if len(path) == 4 and path[0] == "fronts" and path[2] in ("line", "entries"):
            return "front"
        return None
    if kind == "places" and len(path) == 2 and path[0] == "places":
        return "place"
    return None


def pairs(text):
    return [(m.group(1), m.group(2), m.span()) for m in PAIR.finditer(text)]


class Plan:
    """The map's places by id and by exact point, and the sites the migration adds."""

    def __init__(self, country):
        self.at = {}       # id -> (east_km, north_km) as floats
        self.by_point = {}  # (east_km, north_km) -> id
        self.sites = []    # new sites: dicts with id, name, e_text, n_text, (crossing)
        for key in ("settlements", "landmarks", "sites"):
            for p in country.get(key, []):
                pt = (float(p["east_km"]), float(p["north_km"]))
                if p["id"] in self.at:
                    raise ValueError(f"map: place id '{p['id']}' is listed twice")
                self.at[p["id"]] = pt
                self.by_point.setdefault(pt, p["id"])

    def place_for(self, want_id, name, e_text, n_text):
        pt = (float(e_text), float(n_text))
        if pt in self.by_point:
            return self.by_point[pt]
        if want_id in self.at:  # e.g. an object named after a landmark but standing off its point
            want_id += "_site"
        if want_id in self.at:
            raise ValueError(f"site id '{want_id}' is taken by a place at {self.at[want_id]}, not {pt}")
        self.at[want_id] = pt
        self.by_point[pt] = want_id
        self.sites.append({"id": want_id, "name": name, "e": e_text, "n": n_text})
        return want_id


def site_wanted(kind, doc, path):
    """The id and name a new site would take for the block at `path`."""
    if kind == "places":
        p = doc["places"][path[1]]
        return p["id"], p["name"]
    o = doc["objects"][path[1]] if path[0] == "objects" else None
    if len(path) == 2:
        return o["id"], o["name"]
    if path[0] == "objects":
        n = path[3] + 1
        return f"{o['id']}_pos{n}", f"{o['name']}: prepared position {n}"
    f = doc["fronts"][path[1]]
    if path[2] == "line":
        n = path[3] + 1
        return f"{f['faction']}_line{n}", f"{f['name']}: line point {n}"
    if path[2] == "entries":
        eid = f["entries"][path[3]]["id"]
        return f"{f['faction']}_entry_{eid.lower()}", f"{f['name']}: entry point {eid}"
    return f"{f['faction']}_{path[2]}", f"{f['name']}: {path[2].replace('_', ' ')}"


def anchor_text(kind, text, plan):
    """The file's text with every anchored block's coordinates replaced by "at" (blocks kept as they are)."""
    doc = json.loads(text)
    bl = blocks(doc)
    found = pairs(text)
    if len(bl) != len(found):
        raise ValueError(f"{len(bl)} coordinate blocks but {len(found)} east_km/north_km pairs in the text")
    edits = []
    for (path, d), (e_text, n_text, span) in zip(bl, found):
        if float(e_text) != float(d["east_km"]) or float(n_text) != float(d["north_km"]):
            raise ValueError(f"{path}: the text's pair does not match its block")
        if role(kind, path) is None:
            continue
        want, name = site_wanted(kind, doc, path)
        edits.append((span, plan.place_for(want, name, e_text, n_text)))
    for (a, b), pid in reversed(edits):
        text = text[:a] + f'"at": "{pid}"' + text[b:]
    return text, len(edits)


def add_place_map(text, after_key):
    """Adds "place_map": "maps/pannonia.json" on its own line after the top-level key `after_key`'s line."""
    if '"place_map"' in text:
        return text
    m = re.search(r'^(  "' + re.escape(after_key) + r'":[^\n]*\n)', text, re.M)
    if m is None:
        raise ValueError(f"no top-level line for '{after_key}'")
    return text[:m.end()] + f'  "place_map": "{MAP}",\n' + text[m.end():]


def site_line(s):
    parts = [f'"id": {json.dumps(s["id"])}', f'"name": {json.dumps(s["name"], ensure_ascii=False)}',
             f'"east_km": {s["e"]}', f'"north_km": {s["n"]}']
    if s.get("crossing"):
        parts.append(f'"crossing": {json.dumps(s["crossing"])}')
    return "    {" + ", ".join(parts) + "}"


def add_sites(map_text, sites):
    if not sites:
        return map_text
    lines = ",\n".join(site_line(s) for s in sites)
    end = map_text.rstrip()
    if not end.endswith("]\n}") and not end.endswith("]\r\n}"):
        raise ValueError("the map must end with its last list (landmarks, then sites)")
    cut = end.rfind("]")
    if '"sites": [' in map_text:  # partly migrated: the new sites after the map's
        if end.rfind('"sites": [') < end.rfind('"landmarks": ['):
            raise ValueError("the map's sites must be its last list")
        return end[:cut].rstrip() + ",\n" + lines + "\n  ]\n}\n"
    return end[:cut].rstrip() + "\n  ],\n  \"sites\": [\n" + lines + "\n  ]\n}\n"


def link_bridges(plan, theatres_after, crossings):
    """A bridge object's site names its crossing: the nearest within 800 m on the road, else on the railway."""
    by_id = {s["id"]: s for s in plan.sites}
    for doc in theatres_after:
        for o in doc.get("objects", []):
            if o.get("kind") != "bridge" or o.get("at") not in by_id:
                continue
            e, n = plan.at[o["at"]]
            best = None
            for net in ("road", "rail"):
                for c in crossings:
                    if c["net"] != net:
                        continue
                    d = math.hypot(c["at"][0] - e, c["at"][1] - n) * 1000.0
                    if d < BRIDGE_LINK_M and (best is None or d < best[0]):
                        best = (d, c["id"])
                if best is not None:
                    break
            if best is not None:
                by_id[o["at"]]["crossing"] = best[1]


def resolve(kind, doc, at):
    """Every anchored or coordinate position in a migrated (or old) file, by path, as (east_m, north_m) floats."""
    out = {}
    for path, d in blocks(doc):
        out[path] = (float(d["east_km"]) * 1000.0, float(d["north_km"]) * 1000.0)

    def walk(v, path):
        if isinstance(v, dict):
            if "at" in v and isinstance(v["at"], str) and role(kind, path) is not None:
                e, n = at[v["at"]]
                out[path] = (e * 1000.0, n * 1000.0)
            for k, x in v.items():
                walk(x, path + (k,))
        elif isinstance(v, list):
            for i, x in enumerate(v):
                walk(x, path + (i,))

    walk(doc, ())
    return out


def migrate(map_text, theatre_texts, places_text, crossings=None):
    """The migration on texts: (map, [theatres], places) -> the same, anchored. Raises ValueError on anything odd and
    proves before returning that every position resolves to the old km * 1000.0, exactly."""
    plan = Plan(json.loads(map_text))
    new_theatres, moved = [], 0
    for t in theatre_texts:
        nt, k = anchor_text("theatre", t, plan)
        moved += k
        new_theatres.append(add_place_map(nt, "notes") if k else nt)
    new_places, k = anchor_text("places", places_text, plan)
    moved += k
    if k:
        new_places = add_place_map(new_places, "theatre")
    if crossings:
        link_bridges(plan, [json.loads(t) for t in new_theatres], crossings)
    new_map = add_sites(map_text, plan.sites)
    # The proof: every position, read the old way and the new, is the same double.
    at = Plan(json.loads(new_map)).at
    for kind, old, new in [("theatre", o, n) for o, n in zip(theatre_texts, new_theatres)] + [("places", places_text, new_places)]:
        a, b = resolve(kind, json.loads(old), at), resolve(kind, json.loads(new), at)
        if a != b:
            bad = sorted(set(a.items()) ^ set(b.items()))[:3]
            raise ValueError(f"positions differ after anchoring: {bad}")
    return new_map, new_theatres, new_places, plan, moved


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--data", default=str(ROOT / "data"))
    ap.add_argument("--crossings", default="", help="country_dump's JSON, to link bridge sites to their crossings")
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args(argv)
    data = Path(a.data)
    read = lambda rel: (data / rel).read_bytes().decode("utf-8")  # noqa: E731
    crossings = json.loads(Path(a.crossings).read_text(encoding="utf-8"))["crossings"] if a.crossings else None
    new_map, new_theatres, new_places, plan, moved = migrate(read(MAP), [read(t) for t in THEATRES], read(PLACES), crossings)
    print(f"{moved} positions anchored, {len(plan.sites)} sites added, every one the same double as before")
    if a.dry_run or moved == 0:
        return 0
    for rel, text in [(MAP, new_map), (PLACES, new_places)] + list(zip(THEATRES, new_theatres)):
        (data / rel).write_bytes(text.encode("utf-8"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
