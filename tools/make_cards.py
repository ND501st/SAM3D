"""The ground clutter's cards (roadmap T6, docs/scenery/T5_T8_PLAN.md): the alpha textures of a tuft of dry grass, a bundle of
stubble and a stand of reeds, drawn here (our own work: no licence, no download), and the crossed-quad meshes that carry
them (tools/blender/build_cards.py). Thin, abundant plants are drawn as a few crossed cards, as many games do, not as solid
models.

    python tools/make_cards.py            # art/clutter/<name>.png and <name>.glb, and the contact sheet out/cards.png

Each texture is 512 x 512 RGBA (the plant on transparency, its blades drawn with a gradient from a dark foot to a pale tip,
anti-aliased by drawing at twice the size); the colours are October's dry plain, by eye. Then tools/prepare_model.py and
tools/ue_import_model.py take each card as a part (data/scenery/model_picks.json, `root` art/clutter).
"""
import math
import random
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "art" / "clutter"
SIZE = 512
SS = 2  # supersampling


def lerp(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))


def blade(draw, x0, y0, length, lean, width, foot, tip, taper=1.0, rng=None, segments=14):
    """One blade from (x0, y0) up and over by `lean` px, tapering from `width` to `width * (1 - taper)` at the tip."""
    pts = []
    for i in range(segments + 1):
        t = i / segments
        # A quadratic curve: straight up at the foot, bending towards the lean.
        x = x0 + lean * t * t
        y = y0 - length * t
        pts.append((x, y))
    for i in range(segments):
        t0, t1 = i / segments, (i + 1) / segments
        w0 = width * (1.0 - taper * t0 ** 0.9) * 0.5
        w1 = width * (1.0 - taper * t1 ** 0.9) * 0.5
        (xa, ya), (xb, yb) = pts[i], pts[i + 1]
        dx, dy = xb - xa, yb - ya
        n = math.hypot(dx, dy) or 1.0
        px, py = -dy / n, dx / n
        poly = [(xa - px * w0, ya - py * w0), (xa + px * w0, ya + py * w0), (xb + px * w1, yb + py * w1), (xb - px * w1, yb - py * w1)]
        draw.polygon(poly, fill=lerp(foot, tip, (t0 + t1) * 0.5) + (255,))


def canvas():
    return Image.new("RGBA", (SIZE * SS, SIZE * SS), (0, 0, 0, 0))


def finish(img, name):
    img = img.resize((SIZE, SIZE), Image.LANCZOS)
    # The transparent pixels take the colour of their neighbours' average so the mip levels do not fringe dark.
    r, g, b, a = img.split()
    flat = Image.new("RGB", img.size, (120, 105, 60))
    flat.paste(img.convert("RGB"), mask=a.point(lambda v: 255 if v > 8 else 0))
    flat.putalpha(a)
    OUT.mkdir(parents=True, exist_ok=True)
    flat.save(OUT / f"{name}.png")
    return flat


def grass(rng):
    img = canvas()
    d = ImageDraw.Draw(img)
    base_y = SIZE * SS - 4
    for _ in range(40):
        x0 = rng.uniform(0.2, 0.8) * SIZE * SS
        length = rng.uniform(0.40, 0.98) * SIZE * SS
        lean = rng.uniform(-0.16, 0.16) * SIZE * SS * (length / (SIZE * SS))
        width = rng.uniform(0.020, 0.034) * SIZE * SS
        green = rng.random() < 0.25
        foot = (54, 56, 28) if not green else (58, 70, 30)
        tip = (172, 150, 84) if not green else (120, 128, 58)
        tip = lerp(tip, (198, 178, 110), rng.random() * 0.4)
        blade(d, x0, base_y, length, lean, width, foot, tip, taper=1.0)
    return finish(img, "grass_card")


def straw(rng):
    """Short stubble: stalks cut off, a hand high, thick at the foot and a pale flat end."""
    img = canvas()
    d = ImageDraw.Draw(img)
    base_y = SIZE * SS - 4
    for _ in range(16):
        x0 = rng.uniform(0.22, 0.78) * SIZE * SS
        length = rng.uniform(0.22, 0.55) * SIZE * SS
        lean = rng.uniform(-0.05, 0.05) * SIZE * SS
        width = rng.uniform(0.024, 0.036) * SIZE * SS
        foot = (96, 74, 40)
        tip = lerp((196, 168, 104), (222, 198, 140), rng.random() * 0.5)
        blade(d, x0, base_y, length, lean, width, foot, tip, taper=0.25)
    return finish(img, "straw_card")


def reed(rng):
    """Dry reed: tall thin stems, a few long narrow leaves, a dark plume at the top of each stem."""
    img = canvas()
    d = ImageDraw.Draw(img)
    base_y = SIZE * SS - 4
    for _ in range(9):
        x0 = rng.uniform(0.25, 0.75) * SIZE * SS
        length = rng.uniform(0.78, 0.98) * SIZE * SS
        lean = rng.uniform(-0.07, 0.07) * SIZE * SS
        stem = rng.uniform(0.010, 0.016) * SIZE * SS
        blade(d, x0, base_y, length, lean, stem, (118, 96, 52), (176, 150, 90), taper=0.45)
        # The plume: a long dark ellipse-like spike on the top of the stem.
        tx, ty = x0 + lean, base_y - length
        pl = rng.uniform(0.09, 0.14) * SIZE * SS
        blade(d, tx, ty + pl * 0.2, pl, lean * 0.15, rng.uniform(0.028, 0.040) * SIZE * SS, (86, 62, 40), (126, 98, 68), taper=0.9)
        # A leaf or two off the stem's lower half, long and narrow, drooping.
        for side in (-1, 1):
            if rng.random() < 0.7:
                ly = base_y - length * rng.uniform(0.18, 0.5)
                blade(d, x0 + lean * 0.2, ly, rng.uniform(0.25, 0.4) * SIZE * SS, side * rng.uniform(0.08, 0.18) * SIZE * SS,
                      rng.uniform(0.012, 0.018) * SIZE * SS, (130, 108, 60), (184, 158, 96), taper=0.9)
    return finish(img, "reed_card")


def main():
    rng = random.Random(11)
    sheets = [grass(rng), straw(rng), reed(rng)]
    sheet = Image.new("RGB", (SIZE * 3, SIZE), (92, 110, 150))
    for i, im in enumerate(sheets):
        sheet.paste(im.convert("RGB"), (i * SIZE, 0), mask=im.split()[3])
    (ROOT / "out").mkdir(exist_ok=True)
    sheet.save(ROOT / "out" / "cards.png")
    blender = None
    base = Path("C:/Program Files/Blender Foundation")
    found = sorted(base.glob("Blender */blender.exe")) if base.exists() else []
    if found:
        blender = str(found[-1])
    if blender is None:
        print("textures written; Blender not found, the card meshes (.glb) are not")
        return 0
    done = subprocess.run([blender, "--background", "--python", str(ROOT / "tools" / "blender" / "build_cards.py"), "--", str(OUT)],
                          capture_output=True, text=True, check=False)
    ok = [l for l in done.stdout.splitlines() if l.startswith("CARD ")]
    for l in ok:
        print(l)
    if done.returncode != 0 or len(ok) != 3:
        print(done.stdout[-1200:], done.stderr[-1200:])
        raise SystemExit("Blender did not build the cards")
    return 0


if __name__ == "__main__":
    sys.exit(main())
