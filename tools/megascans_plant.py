"""Prepares a Megascans plant (an FBX download from Fab: LOD0-3 per variant, LOD3 a billboard, its maps as JPGs) for the
model pipeline (docs/scenery/VEGETATION.md section 4): one GLB per level picked, each with its colour and opacity in one
RGBA texture (M_SimFoliage reads the alpha as its mask). A billboard level is made two crossed cards, so a plant turned
by its own yaw is never seen edge-on. Then tools/prepare_model.py and tools/ue_import_model.py take the GLBs as picks.

    python tools/megascans_plant.py art/thirdparty/models/elderberry_megascans VarA 2 3

writes <folder>/prepared/<VarA>_LOD2.glb and <VarA>_LOD3.glb (and the RGBA PNGs, at 1024 px). The download's files are
data: nothing in the folder is run.
"""
import subprocess
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
SIZE = 1024


def rgba(folder, kind):
    """The colour map with the opacity map as alpha: `kind` '' for the atlas, 'Billboard_' for the billboard."""
    colour = next(folder.glob(f"*_{kind}BaseColor.jpg"))
    opacity = next(folder.glob(f"*_{kind}Opacity.jpg"))
    out = folder / "prepared" / f"{kind or 'Atlas_'}RGBA.png"
    out.parent.mkdir(exist_ok=True)
    c = Image.open(colour).convert("RGB").resize((SIZE, SIZE), Image.LANCZOS)
    a = Image.open(opacity).convert("L").resize((SIZE, SIZE), Image.LANCZOS)
    c.putalpha(a)
    c.save(out)
    return out


def blender():
    found = sorted(Path("C:/Program Files/Blender Foundation").glob("Blender */blender.exe"))
    if not found:
        raise SystemExit("Blender not found")
    return str(found[-1])


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        return 2
    folder = (ROOT / sys.argv[1]).resolve()
    var = sys.argv[2]
    lods = [int(x) for x in sys.argv[3:]]
    atlas = rgba(folder, "")
    board = rgba(folder, "Billboard_")
    for lod in lods:
        fbx = next(folder.glob(f"*_{var}_LOD{lod}.fbx"))
        out = folder / "prepared" / f"{var}_LOD{lod}.glb"
        cross = lod == 3
        args = [blender(), "--background", "--python", str(ROOT / "tools" / "blender" / "megascans_plant.py"), "--", str(fbx),
                str(board if cross else atlas), str(out)] + (["--cross"] if cross else [])
        done = subprocess.run(args, capture_output=True, text=True, check=False)
        line = [l for l in done.stdout.splitlines() if l.startswith("PLANT ")]
        if done.returncode != 0 or not line:
            print(done.stdout[-1500:], done.stderr[-1500:])
            raise SystemExit(f"Blender did not prepare {fbx.name}")
        print(line[0])
    return 0


if __name__ == "__main__":
    sys.exit(main())
