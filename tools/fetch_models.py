"""Fetch a free 3D model onto this machine, under art/thirdparty/models/<id>/ (ignored by git: third-party files are fetched
per machine, docs/scenery/ASSETS.md). Roadmap T5-T8, docs/scenery/T5_T8_PLAN.md.

    python tools/fetch_models.py polyhaven wooden_military_crate --sizes     # what a fetch would download (nothing fetched)
    python tools/fetch_models.py polyhaven wooden_military_crate --res 1k    # fetch it: the glTF, its buffer and its textures

Only Poly Haven's public API (api.polyhaven.com, CC0-1.0) is fetched from: Fab and Sketchfab need accounts, so their files
are put in a folder by the owner (the import tool reads them from there). One model a call, named on the command line: the
owner says which, as the asset register's rule is (nothing is fetched otherwise). Every file's MD5 is checked against the
site's. The result is a folder holding <id>.gltf, its .bin and textures, and source.json (origin, authors, licence, files'
SHA-256) for the register.
"""
import argparse
import hashlib
import json
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "art" / "thirdparty" / "models"
API = "https://api.polyhaven.com"
USER_AGENT = "SAM3D-fetch-models/1.0 (a hobby flight-sim project; CC0 models)"


def get_json(url):
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(req, timeout=60) as r:
        return json.load(r)


def get_bytes(url):
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(req, timeout=300) as r:
        return r.read()


def listing(asset, res):
    """(info, [(relative path, url, size, md5)]) of the glTF at a resolution; raises SystemExit where there is none."""
    info = get_json(f"{API}/info/{asset}")
    files = get_json(f"{API}/files/{asset}")
    if info.get("type") != 2:
        raise SystemExit(f"{asset} is not a model on Poly Haven")
    by_res = files.get("gltf", {})
    if res not in by_res:
        raise SystemExit(f"{asset} has no {res} glTF (it has {', '.join(sorted(by_res))})")
    main = by_res[res]["gltf"]
    out = [(f"{asset}.gltf", main["url"], main.get("size", 0), main.get("md5", ""))]
    for rel, f in main.get("include", {}).items():
        out.append((rel, f["url"], f.get("size", 0), f.get("md5", "")))
    return info, out


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("site", choices=("polyhaven",))
    ap.add_argument("asset", help="the site's own name for the model, as in its page's address")
    ap.add_argument("--res", default="1k", help="texture resolution (1k, 2k, 4k)")
    ap.add_argument("--sizes", action="store_true", help="say what a fetch would download and fetch nothing")
    args = ap.parse_args(argv)
    info, files = listing(args.asset, args.res)
    total = sum(f[2] for f in files)
    authors = ", ".join(info.get("authors", {}).keys())
    print(f"{info.get('name')} ({args.asset}) by {authors}: CC0-1.0, {len(files)} files, {total / 1e6:.1f} MB at {args.res}")
    for rel, _url, size, _md5 in files:
        print(f"  {rel}  {size / 1e6:.2f} MB")
    if args.sizes:
        return 0
    folder = OUT / args.asset
    if folder.exists() and any(folder.iterdir()):
        raise SystemExit(f"{folder} is not empty: fetch into a new folder (delete it first)")
    folder.mkdir(parents=True, exist_ok=True)
    sums = {}
    for rel, url, _size, md5 in files:
        data = get_bytes(url)
        if md5 and hashlib.md5(data).hexdigest() != md5:
            raise SystemExit(f"{rel}: its MD5 does not match the site's")
        dest = folder / rel
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(data)
        sums[rel] = hashlib.sha256(data).hexdigest()
    (folder / "source.json").write_text(json.dumps({
        "origin": f"https://polyhaven.com/a/{args.asset}", "authors": list(info.get("authors", {}).keys()), "licence": "CC0-1.0",
        "resolution": args.res, "files": sums}, indent=2), encoding="utf-8")
    print(f"fetched into {folder}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
