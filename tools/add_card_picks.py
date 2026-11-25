"""Adds the picks for a card tree (tools/blender/card_tree.py) to data/scenery/model_picks.json: python tools/add_card_picks.py hornbeam 'Quixel Megaplants: European Hornbeam'"""
import json
import sys
from pathlib import Path

species, source = sys.argv[1], sys.argv[2]
p = Path(__file__).resolve().parent.parent / "data" / "scenery" / "model_picks.json"
d = json.loads(p.read_text(encoding="utf-8"))
base = {"folder": f"card_{species}", "part": f"tree_{species}", "frame": "tree", "origin": "own generator (tools/blender/card_tree.py); leaf photographs from " + source + " (Fab listing in the owner's library)",
        "author": "SAM3D (generator); leaves: Quixel Megascans", "licence": "Fab-Standard", "fetched": "2026-11-05", "foliage": True}
levels = [("", 0, "the near level (about 420 triangles): 0-150 m"), ("_l1", 1, "the far level (about 170 triangles): 150-600 m")]
d["picks"] = [x for x in d["picks"] if not x["id"].startswith(f"{species}_cards")]
for suffix, lod, what in levels:
    d["picks"].append(dict(base, id=f"{species}_cards{suffix}", file=f"prepared/{species}_L{lod}.glb", variant=lod,
                           notes=f"A {species} of twig cards (leaf photographs composed into twigs by tools/leaf_atlas.py), {what}. Fab Standard licence for the leaf photographs (not redistributable): files under art/thirdparty and /Game/ThirdParty, ignored by git."))
p.write_text(json.dumps(d, indent=2, ensure_ascii=False), encoding="utf-8")
print("picks added for", species)
