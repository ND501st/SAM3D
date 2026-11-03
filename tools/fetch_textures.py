"""Fetch the ground's and the buildings' textures named in the texture register (data/scenery/textures.json, roadmap T1,
docs/scenery/TERRAIN_ROADMAP.md) onto this machine, under art/thirdparty/textures/<id>/ (ignored by git: the files are
fetched per machine, as docs/scenery/ASSETS.md says for every third-party file).

    python tools/fetch_textures.py --sizes         # what a fetch would download, from the sites' APIs (nothing fetched)
    python tools/fetch_textures.py                 # every texture in the register not yet on this machine
    python tools/fetch_textures.py --list          # what the register holds and what is here, nothing fetched
    python tools/fetch_textures.py --ids a,b       # only these
    python tools/fetch_textures.py --record        # write each file's SHA-256 into the register where none is recorded

Every texture is free: CC0-1.0 from Poly Haven (its public API, api.polyhaven.com) or ambientCG (its zip download),
the only sites this tool fetches from. Each texture lands as albedo, normal (DirectX convention, as Unreal reads it),
roughness and, when the site has one, ao, as the site serves them (JPEG: a quarter of PNG's size, and Unreal compresses
them again), and orm.png made from them (R the occlusion, white without one; G the roughness), which the ground
material reads. A file whose SHA-256 differs from the register's is refused. Packing needs Pillow (pip install pillow).
Then tools/ue_build_ground_material.py imports them into Unreal.
"""
import argparse
import hashlib
import io
import json
import sys
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REGISTER = ROOT / "data" / "scenery" / "textures.json"
OUT = ROOT / "art" / "thirdparty" / "textures"
SITES = ("polyhaven", "ambientcg")
LICENCES = ("CC0-1.0", "CC-BY-4.0")
MAPS = ("albedo", "normal", "roughness", "ao")
RESOLUTIONS = ("1k", "2k", "4k")
USER_AGENT = "SAM3D-fetch-textures/1.0 (a hobby flight-sim project; CC0 textures)"


def check_register(register):
    """The register's problems (a list of strings): ids, sites, licences, resolutions."""
    out = []
    seen = set()
    for t in register.get("textures", []):
        tid = t.get("id", "")
        if not tid or not tid.replace("_", "").isalnum() or tid.lower() != tid:
            out.append(f"{tid!r}: an id is lower-case letters, digits and underscores")
        if tid in seen:
            out.append(f"{tid}: twice")
        seen.add(tid)
        if t.get("site") not in SITES:
            out.append(f"{tid}: site {t.get('site')!r} is not one of {', '.join(SITES)}")
        if not t.get("asset"):
            out.append(f"{tid}: no asset (the site's own name for it)")
        if t.get("licence") not in LICENCES:
            out.append(f"{tid}: licence {t.get('licence')!r} is not one of {', '.join(LICENCES)}")
        if t.get("licence") == "CC-BY-4.0" and not t.get("author"):
            out.append(f"{tid}: CC-BY needs its author, for the credits")
        if t.get("resolution", "2k") not in RESOLUTIONS:
            out.append(f"{tid}: resolution {t.get('resolution')!r} is not one of {', '.join(RESOLUTIONS)}")
        if not t.get("origin", "").startswith("https://"):
            out.append(f"{tid}: no origin (the page it comes from)")
        scan = t.get("scan_m", 1.0)
        if isinstance(scan, bool) or not isinstance(scan, (int, float)) or scan <= 0:
            out.append(f"{tid}: scan_m (the scan's real width) is not a positive number of metres")
        crop = t.get("crop_u")
        if crop is not None and not (isinstance(crop, list) and len(crop) == 2
                                     and all(isinstance(v, (int, float)) and not isinstance(v, bool) for v in crop)
                                     and 0.0 <= crop[0] and crop[1] <= 1.0 and crop[1] - crop[0] >= 0.25):
            out.append(f"{tid}: crop_u is [from, to] across the scan's width, 0..1, at least a quarter of it")
    return out


def polyhaven_files(files_json, resolution):
    """Each map's file from Poly Haven's /files/<asset> answer: {map: {"url", "size"}}. Albedo, normal (DirectX) and
    roughness are needed; AO is taken when there is one. JPEG preferred, PNG otherwise."""
    keys = {"albedo": ("Diffuse", "diff"), "normal": ("nor_dx",), "roughness": ("Rough", "rough"), "ao": ("AO", "ao")}
    out = {}
    for name, candidates in keys.items():
        for k in candidates:
            by_res = files_json.get(k)
            if not isinstance(by_res, dict) or resolution not in by_res:
                continue
            formats = by_res[resolution]
            for fmt in ("jpg", "png"):
                if fmt in formats and "url" in formats[fmt]:
                    out[name] = {"url": formats[fmt]["url"], "size": formats[fmt].get("size", 0)}
                    break
            if name in out:
                break
    missing = [m for m in ("albedo", "normal", "roughness") if m not in out]
    if missing:
        raise ValueError(f"Poly Haven has no {', '.join(missing)} at {resolution}")
    return out


def polyhaven_urls(files_json, resolution):
    """The download URL of each map: {map: url} (polyhaven_files)."""
    return {m: f["url"] for m, f in polyhaven_files(files_json, resolution).items()}


def ambientcg_zip_url(asset, resolution):
    """ambientCG's zip of an asset at a resolution, JPEG: https://ambientcg.com/get?file=<asset>_2K-JPG.zip"""
    return f"https://ambientcg.com/get?file={asset}_{resolution.upper()}-JPG.zip"


def ambientcg_members(names):
    """The map each member of ambientCG's zip is: {map: member}. Its normal is taken in the DirectX convention."""
    out = {}
    for n in names:
        stem, dot, ext = n.lower().rpartition(".")
        if not dot or ext not in ("png", "jpg"):
            continue
        if stem.endswith("_color"):
            out["albedo"] = n
        elif stem.endswith("_normaldx"):
            out["normal"] = n
        elif stem.endswith("_roughness"):
            out["roughness"] = n
        elif stem.endswith("_ambientocclusion"):
            out["ao"] = n
    missing = [m for m in ("albedo", "normal", "roughness") if m not in out]
    if missing:
        raise ValueError(f"ambientCG's zip has no {', '.join(missing)}")
    return out


def pack_orm(roughness, ao=None, size=2048):
    """orm.png's bytes from the roughness and occlusion maps' bytes: R the occlusion (white without one), G the
    roughness, B 0, `size` square (the ground's texture arrays take one size)."""
    from PIL import Image  # only the packing needs Pillow

    def grey(data):
        img = Image.open(io.BytesIO(data)).convert("L")
        return img if img.size == (size, size) else img.resize((size, size), Image.LANCZOS)

    r = grey(roughness)
    o = grey(ao) if ao is not None else Image.new("L", (size, size), 255)
    out = io.BytesIO()
    Image.merge("RGB", (o, r, Image.new("L", (size, size), 0))).save(out, format="PNG")
    return out.getvalue()


def crop_u(data, lo, hi):
    """A map's PNG bytes cut to [lo, hi] of its width (its U) and stretched back to its size: for a scan whose middle
    alone tiles along U, as farm_furrows' ridges do between the trench and grass at each side."""
    from PIL import Image  # only the cropping needs Pillow

    img = Image.open(io.BytesIO(data))
    w, h = img.size
    out = io.BytesIO()
    img.crop((round(lo * w), 0, round(hi * w), h)).resize((w, h), Image.LANCZOS).save(out, format="PNG")
    return out.getvalue()


def download_size(t):
    """What fetching the texture downloads, bytes, from the site's API (nothing else fetched)."""
    res = t.get("resolution", "2k")
    if t["site"] == "polyhaven":
        files = json.loads(fetch(f"https://api.polyhaven.com/files/{t['asset']}"))
        return sum(f["size"] for f in polyhaven_files(files, res).values())
    api = json.loads(fetch(f"https://ambientcg.com/api/v3/assets?id={t['asset']}&include=downloads"))
    want = ambientcg_zip_url(t["asset"], res)
    for asset in api.get("assets", []):
        for d in asset.get("downloads", []):
            if d.get("url") == want:
                return d.get("size", 0)
    raise ValueError(f"ambientCG lists no {want}")


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def fetch(url):
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(req, timeout=120) as r:
        return r.read()


def maps_of(t):
    """The texture's maps as {map: bytes}, from its site."""
    res = t.get("resolution", "2k")
    if t["site"] == "polyhaven":
        files = json.loads(fetch(f"https://api.polyhaven.com/files/{t['asset']}"))
        return {m: fetch(u) for m, u in polyhaven_urls(files, res).items()}
    with zipfile.ZipFile(io.BytesIO(fetch(ambientcg_zip_url(t["asset"], res)))) as z:
        return {m: z.read(n) for m, n in ambientcg_members(z.namelist()).items()}


def extension(data):
    """A map's file extension by its bytes: .png or .jpg (Poly Haven serves JPG where it has no PNG)."""
    if data[:8] == b"\x89PNG\r\n\x1a\n":
        return ".png"
    if data[:3] == b"\xff\xd8\xff":
        return ".jpg"
    raise ValueError("neither PNG nor JPEG")


def here(t):
    folder = OUT / t["id"]
    return (folder / "orm.png").exists() and all((folder / f"{m}.png").exists() or (folder / f"{m}.jpg").exists()
                                              for m in ("albedo", "normal", "roughness"))


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--ids", default="")
    ap.add_argument("--record", action="store_true")
    ap.add_argument("--sizes", action="store_true")
    args = ap.parse_args(argv)
    register = json.loads(REGISTER.read_text(encoding="utf-8"))
    problems = check_register(register)
    for p in problems:
        print(p)
    if problems:
        return 1
    wanted = set(filter(None, args.ids.split(",")))
    textures = [t for t in register.get("textures", []) if not wanted or t["id"] in wanted]
    if args.list:
        for t in textures:
            print(f"{t['id']:28} {t['site']:9} {t['asset']:32} {t['licence']:9} {'here' if here(t) else 'not here'}")
        return 0
    if args.sizes:
        total = 0
        for t in textures:
            try:
                n = download_size(t)
            except Exception as e:  # noqa: BLE001 - one texture's failure is reported, the rest go on
                print(f"{t['id']:16} {t['site']:9} {t['asset']:18} size not known ({e})")
                continue
            total += n
            print(f"{t['id']:16} {t['site']:9} {t['asset']:18} {n / 1e6:6.1f} MB  {t['origin']}")
        print(f"{len(textures)} textures, {total / 1e6:.1f} MB to download")
        return 0
    changed = False
    for t in textures:
        if here(t) and not args.record:
            print(f"{t['id']}: here")
            continue
        try:
            maps = maps_of(t)
        except Exception as e:  # noqa: BLE001 - one texture's failure is reported, the rest go on
            print(f"{t['id']}: not fetched ({e})")
            continue
        recorded = t.get("sha256", {})
        bad = [m for m, data in maps.items() if m in recorded and recorded[m] != sha256(data)]
        if bad:
            print(f"{t['id']}: refused, {', '.join(bad)} differ from the register's SHA-256")
            continue
        try:
            named = {m: (data, extension(data)) for m, data in maps.items()}
        except ValueError as e:
            print(f"{t['id']}: refused ({e})")
            continue
        try:
            if "crop_u" in t:
                named = {m: (crop_u(data, *t["crop_u"]), ".png") for m, (data, _) in named.items()}
            orm = pack_orm(named["roughness"][0], named["ao"][0] if "ao" in named else None)
        except ImportError:
            print(f"{t['id']}: not written, its maps need Pillow (pip install pillow)")
            continue
        folder = OUT / t["id"]
        folder.mkdir(parents=True, exist_ok=True)
        for m in MAPS:
            for ext in (".png", ".jpg"):
                (folder / f"{m}{ext}").unlink(missing_ok=True)
        for m, (data, ext) in named.items():
            (folder / f"{m}{ext}").write_bytes(data)
        (folder / "orm.png").write_bytes(orm)
        if args.record:
            t["sha256"] = {m: recorded.get(m, sha256(data)) for m, data in maps.items()}
            changed = True
        print(f"{t['id']}: {', '.join(sorted(maps))} ({sum(len(d) for d in maps.values()) / 1e6:.1f} MB)")
    if changed:
        REGISTER.write_text(json.dumps(register, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        print(f"recorded the SHA-256s in {REGISTER.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
