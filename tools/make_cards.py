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


def canvas(w=SIZE, h=SIZE):
    return Image.new("RGBA", (w * SS, h * SS), (0, 0, 0, 0))


def finish(img, name, w=SIZE, h=SIZE, fill=(120, 105, 60)):
    img = img.resize((w, h), Image.LANCZOS)
    # The transparent pixels take the colour of their neighbours' average so the mip levels do not fringe dark.
    r, g, b, a = img.split()
    flat = Image.new("RGB", img.size, fill)
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


MAIZE_W, MAIZE_H = 512, 2048  # the card stands 0.55-0.8 m wide and 1.6-2.6 m tall: about a quarter as wide as tall


def leaf(d, x0, y0, length, side, out_deg, hang_deg, width, foot, tip, rng):
    """A dry maize leaf: out from the stalk at (x0, y0), its heading from straight up turning from `out_deg` to `hang_deg`
    (over 90: it hangs), narrowing to a point and narrower where it twists; `side` -1 left, 1 right."""
    segments = 22
    pts = [(x0, y0)]
    a0 = math.radians(out_deg)
    a1 = math.radians(hang_deg)
    ds = length / segments
    x, y = x0, y0
    for i in range(segments):
        t = (i + 0.5) / segments
        a = a0 + (a1 - a0) * t ** 0.9
        x += side * math.sin(a) * ds
        y -= math.cos(a) * ds
        pts.append((x, y))
    twist = rng.uniform(0.0, 6.0)
    for i in range(segments):
        t0, t1 = i / segments, (i + 1) / segments
        # Widest a third of the way out, to a point; narrower where the dry leaf twists.
        def wid(t):
            return width * (0.45 + 0.55 * math.sin(math.pi * min(1.0, t * 1.6))) * (1.0 - t) ** 0.7 * (0.6 + 0.4 * abs(math.cos(t * 5.0 + twist)))
        (xa, ya), (xb, yb) = pts[i], pts[i + 1]
        dx, dy = xb - xa, yb - ya
        n = math.hypot(dx, dy) or 1.0
        px, py = -dy / n, dx / n
        w0, w1 = wid(t0) * 0.5, wid(t1) * 0.5
        poly = [(xa - px * w0, ya - py * w0), (xa + px * w0, ya + py * w0), (xb + px * w1, yb + py * w1), (xb - px * w1, yb - py * w1)]
        d.polygon(poly, fill=lerp(foot, tip, (t0 + t1) * 0.5) + (255,))


def tint(img, tone):
    """The card a shade lighter (tone over 1: towards a pale buff, as a plant dried out sooner) or darker (under 1:
    towards a damp brown); the alpha kept."""
    if tone == 1.0:
        return img
    r, g, b, a = img.split()
    if tone > 1.0:
        lift = tone - 1.0
        bands = [ch.point(lambda v, p=p: int(round(v + (p - v) * lift * 1.6))) for ch, p in zip((r, g, b), (236, 218, 172))]
    else:
        bands = [ch.point(lambda v, k=k: int(round(v * (tone - (1.0 - tone) * k)))) for ch, k in zip((r, g, b), (0.0, 0.15, 0.4))]
    return Image.merge("RGBA", bands + [a])


def maize(rng, name="maize_card", limp=0.35, same_side=0.0, broken=0.0, heights=(0.84, 0.99), tone=1.0):
    """Standing maize dried before the harvest: two or three plants, a pale stalk the card's height thickening to the foot,
    a dozen dry leaves along it hanging and curled (grey-buff, the lower ones darkest), an ear in its husk hanging from the
    middle, a thin tassel at the top. The card is stretched about four times as tall as wide, so it is drawn on a tall
    canvas. The second card (maize_card_b) breaks the first's order: more leaves limp, a leaf now and then on the same
    side as the last, and a plant's top broken over (`limp`, `same_side`, `broken`: shares; at 0 the first card's draws).
    The later cards (c-f) vary the plants' heights on the card (`heights`, shares of the card's height: a plant under
    0.84 is stunted, its leaves, stalk and ear smaller with it) and the card's shade (`tone`, `tint`), so a row of them is
    ragged and uneven as a real dry field is (a plant on poor ground or late sown stays short)."""
    W, H = MAIZE_W * SS, MAIZE_H * SS
    img = canvas(MAIZE_W, MAIZE_H)
    d = ImageDraw.Draw(img)
    base_y = H - 4
    plants = rng.choice([2, 3])
    xs = [W * (0.5 + (i - (plants - 1) / 2.0) * 0.26 + rng.uniform(-0.05, 0.05)) for i in range(plants)]
    for x0 in xs:
        height = rng.uniform(heights[0], heights[1]) * H
        # A stunted plant is smaller all over (1.0 for a full one: the first two cards draw as before).
        scale = min(1.0, height / (0.84 * H))
        lean = rng.uniform(-0.06, 0.06) * W
        stalk_w = rng.uniform(0.030, 0.040) * W * (0.6 + 0.4 * scale)
        snapped = broken > 0 and rng.random() < broken
        # The stalk: buff, darker at the foot; a snapped one bends over at three quarters of its height and hangs.
        if snapped:
            at = rng.uniform(0.68, 0.80)
            blade(d, x0, base_y, height * at, lean * at * at, stalk_w, (112, 92, 58), (180, 160, 114), taper=0.45, segments=20)
            kx, ky = x0 + lean * at * at, base_y - height * at
            side_k = rng.choice([-1, 1])
            leaf(d, kx, ky, height * (1.0 - at) * 1.1, side_k, rng.uniform(25.0, 50.0), rng.uniform(150.0, 175.0), stalk_w * 1.6,
                 (170, 150, 108), (150, 132, 98), rng)
        else:
            blade(d, x0, base_y, height, lean, stalk_w, (112, 92, 58), (186, 166, 120), taper=0.75, segments=24)
        # The leaves, alternate sides up the stalk, from a tenth of the way up to the tassel.
        n = rng.randint(10, 13)
        top_t = (at - 0.03) if snapped else 0.88
        side = -1
        for i in range(n):
            t = 0.10 + (top_t - 0.10) * (i + rng.uniform(-0.3, 0.3)) / n
            sx = x0 + lean * t * t
            sy = base_y - height * t
            side = -side if not (same_side > 0 and rng.random() < same_side) else side
            # A leaf is some 0.6-0.9 m long: on a card 0.65 m wide and 2.4 m tall, about a quarter of the height.
            length = rng.uniform(0.14, 0.22) * H * (1.1 - 0.5 * abs(t - 0.45)) * scale
            low = 1.0 - t
            # Dry: buff to grey-brown, the lower leaves darker and greyer; one in three gone limp down the stalk.
            grey = rng.random() * 0.5
            foot = lerp(lerp((150, 126, 86), (128, 118, 100), grey), (90, 78, 58), low * 0.8)
            tip = lerp(lerp((196, 178, 136), (170, 160, 140), grey), (120, 104, 80), low * 0.7)
            if rng.random() < limp:
                out_deg, hang_deg, length = rng.uniform(8.0, 22.0), rng.uniform(165.0, 180.0), length * 0.8
            else:
                out_deg, hang_deg = rng.uniform(22.0, 45.0), rng.uniform(115.0, 175.0)
            # Keep the leaf on the card: it hangs on the side with room.
            if (side < 0 and sx < 0.25 * W) or (side > 0 and sx > 0.75 * W):
                side = -side
            leaf(d, sx, sy, length, side, out_deg, hang_deg, rng.uniform(0.06, 0.10) * W * scale ** 0.5, foot, tip, rng)
        # The ear: hanging from the middle at 30-50 degrees off the stalk, a spindle in its husk.
        t = rng.uniform(0.40, 0.52)
        ex, ey = x0 + lean * t * t, base_y - height * t
        side = rng.choice([-1, 1])
        ear_l = rng.uniform(0.07, 0.09) * H * scale
        ang = math.radians(rng.uniform(130.0, 150.0))  # from straight up: hanging
        ux, uy = side * math.sin(ang), -math.cos(ang)
        vx, vy = -uy, ux
        ear_w = rng.uniform(0.050, 0.065) * W * scale ** 0.5
        outline = []
        for k in range(13):
            s = k / 12.0
            r = ear_w * math.sin(math.pi * min(1.0, 0.15 + 0.85 * s)) ** 0.7
            outline.append((ex + ux * ear_l * s + vx * r, ey + uy * ear_l * s + vy * r))
        for k in range(12, -1, -1):
            s = k / 12.0
            r = ear_w * math.sin(math.pi * min(1.0, 0.15 + 0.85 * s)) ** 0.7
            outline.append((ex + ux * ear_l * s - vx * r, ey + uy * ear_l * s - vy * r))
        d.polygon(outline, fill=(160, 136, 94, 255))
        # The tassel: a few thin spikes from the top (a snapped plant's hangs with its top).
        if snapped:
            continue
        tx, ty = x0 + lean, base_y - height
        for _ in range(rng.randint(4, 7)):
            blade(d, tx, ty + 6, rng.uniform(0.03, 0.06) * H * scale, rng.uniform(-0.12, 0.12) * W, rng.uniform(0.006, 0.010) * W, (150, 128, 88), (176, 156, 116), taper=0.8)
    fill = tint(Image.new("RGBA", (1, 1), (150, 130, 92, 255)), tone).getpixel((0, 0))[:3]
    return finish(tint(img, tone), name, MAIZE_W, MAIZE_H, fill=fill)


def sunflower(rng, name="sunflower_card", heights=(0.84, 0.99), tone=1.0, plants=2):
    """Standing sunflower dried before the harvest (plot 4): a thick grey-brown stalk, a few shrivelled near-black leaves
    hanging close down it, and at the top the neck bent over so the heavy head hangs face down: its back a dull tan
    disc, a ring of dry bracts round it, a rim of the black face showing. On the same tall canvas as the maize's."""
    W, H = MAIZE_W * SS, MAIZE_H * SS
    img = canvas(MAIZE_W, MAIZE_H)
    d = ImageDraw.Draw(img)
    base_y = H - 4
    xs = [W * (0.5 + (i - (plants - 1) / 2.0) * 0.36 + rng.uniform(-0.05, 0.05)) for i in range(plants)]
    for x0 in xs:
        height = rng.uniform(heights[0], heights[1]) * H
        scale = min(1.0, height / (0.84 * H))
        lean = rng.uniform(-0.05, 0.05) * W
        stalk_w = rng.uniform(0.045, 0.060) * W * (0.6 + 0.4 * scale)
        neck_at = rng.uniform(0.80, 0.86)
        blade(d, x0, base_y, height * neck_at, lean * neck_at * neck_at, stalk_w, (42, 35, 28), (76, 64, 50), taper=0.35, segments=22)
        # The leaves: shrivelled, dark, hanging close down the stalk from its lower two thirds.
        for i in range(rng.randint(5, 8)):
            t = rng.uniform(0.12, 0.70)
            sx, sy = x0 + lean * t * t, base_y - height * t
            leaf(d, sx, sy, rng.uniform(0.05, 0.11) * H * scale, rng.choice([-1, 1]), rng.uniform(8.0, 25.0), rng.uniform(160.0, 182.0),
                 rng.uniform(0.04, 0.07) * W * scale ** 0.5, (30, 25, 20), (60, 50, 38), rng)
        # The neck bends over from the top of the stalk; the head hangs at its end.
        side = rng.choice([-1, 1])
        if (side < 0 and x0 < 0.3 * W) or (side > 0 and x0 > 0.7 * W):
            side = -side
        nx, ny = x0 + lean * neck_at * neck_at, base_y - height * neck_at
        neck_len = height * (1.0 - neck_at) * 1.2
        bend = math.radians(rng.uniform(110.0, 160.0))
        pts = [(nx, ny)]
        x, y = nx, ny
        for k in range(12):
            a = bend * ((k + 0.5) / 12) ** 1.3
            x += side * math.sin(a) * neck_len / 12
            y -= math.cos(a) * neck_len / 12
            pts.append((x, y))
        for k in range(12):
            (xa, ya), (xb, yb) = pts[k], pts[k + 1]
            w = stalk_w * (0.85 - 0.25 * k / 12) * 0.5
            dx, dy = xb - xa, yb - ya
            n = math.hypot(dx, dy) or 1.0
            px, py = -dy / n * w, dx / n * w
            d.polygon([(xa - px, ya - py), (xa + px, ya + py), (xb + px, yb + py), (xb - px, yb - py)], fill=(58, 48, 36, 255))
        # The head: a disc seen from its back and edge, turned with the neck's end; its face's black rim below.
        hx, hy = pts[-1]
        r = rng.uniform(0.15, 0.20) * W * (0.7 + 0.3 * scale)
        flat = rng.uniform(0.45, 0.65)  # the disc's tilt to the eye
        axis = bend + rng.uniform(-0.2, 0.2)
        ux, uy = side * math.sin(axis), -math.cos(axis)  # the neck's way out: the disc's face looks along it
        vx, vy = -uy, ux
        cx, cy = hx + ux * r * flat * 0.6, hy + uy * r * flat * 0.6

        def disc(rad, f, shift):
            return [(cx + ux * shift + vx * rad * math.cos(t) + ux * rad * f * math.sin(t), cy + uy * shift + vy * rad * math.cos(t) + uy * rad * f * math.sin(t))
                    for t in [2.0 * math.pi * k / 40 for k in range(40)]]

        # Bracts: a ring of small dry points round the back.
        for k in range(18):
            t = 2.0 * math.pi * k / 18 + rng.uniform(-0.1, 0.1)
            bx, by = cx + vx * r * math.cos(t) + ux * r * flat * math.sin(t), cy + vy * r * math.cos(t) + uy * r * flat * math.sin(t)
            ox, oy = vx * math.cos(t) + ux * flat * math.sin(t), vy * math.cos(t) + uy * flat * math.sin(t)
            tip = r * rng.uniform(0.18, 0.32)
            d.polygon([(bx - oy * r * 0.08, by + ox * r * 0.08), (bx + oy * r * 0.08, by - ox * r * 0.08), (bx + ox * tip, by + oy * tip)],
                      fill=(50, 40, 30, 255))
        d.polygon(disc(r, flat, r * flat * 0.25), fill=(20, 17, 15, 255))     # the face's rim
        d.polygon(disc(r * 0.97, flat, 0.0), fill=(80, 66, 48, 255))       # the back
        d.polygon(disc(r * 0.55, flat, -r * flat * 0.1), fill=(68, 56, 42, 255))
    fill = tint(Image.new("RGBA", (1, 1), (52, 44, 34, 255)), tone).getpixel((0, 0))[:3]
    return finish(tint(img, tone), name, MAIZE_W, MAIZE_H, fill=fill)


def main():
    rng = random.Random(11)
    sheets = [grass(rng), straw(rng), reed(rng)]
    talls = [maize(random.Random(23)), maize(random.Random(41), "maize_card_b", limp=0.5, same_side=0.3, broken=0.45),
             # Stunted, a shade darker: plants on a wet hollow or sown late, half to two thirds of the stand.
             maize(random.Random(57), "maize_card_c", limp=0.45, heights=(0.48, 0.66), tone=0.84),
             # The tallest, a shade paler: dried out first on the high ground.
             maize(random.Random(73), "maize_card_d", limp=0.3, heights=(0.95, 1.0), tone=1.12),
             # Mixed: one plant full, one barely half (each draws its own height across the range).
             maize(random.Random(89), "maize_card_e", limp=0.4, same_side=0.2, heights=(0.45, 1.0), tone=0.92),
             # Middling and pale, more leaves hanging, a top snapped now and then.
             maize(random.Random(97), "maize_card_f", limp=0.6, broken=0.3, heights=(0.66, 0.84), tone=1.07),
             # Sunflower (plot 4): even, uneven and short-and-dark.
             sunflower(random.Random(131)),
             sunflower(random.Random(137), "sunflower_card_b", heights=(0.55, 1.0), tone=0.9),
             sunflower(random.Random(149), "sunflower_card_c", heights=(0.6, 0.8), tone=1.1, plants=3)]
    sheet = Image.new("RGB", (SIZE * 3 + len(talls) * MAIZE_W // 4, SIZE), (92, 110, 150))
    for i, im in enumerate(sheets):
        sheet.paste(im.convert("RGB"), (i * SIZE, 0), mask=im.split()[3])
    for i, tall in enumerate(talls):
        small = tall.resize((MAIZE_W // 4, SIZE), Image.LANCZOS)
        sheet.paste(small.convert("RGB"), (SIZE * 3 + i * MAIZE_W // 4, 0), mask=small.split()[3])
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
    if done.returncode != 0 or len(ok) != 12:
        print(done.stdout[-1200:], done.stderr[-1200:])
        raise SystemExit("Blender did not build the cards")
    return 0


if __name__ == "__main__":
    sys.exit(main())
