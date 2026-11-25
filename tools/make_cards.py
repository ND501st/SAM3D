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


def ellipse_leaf(d, cx, cy, length, width, angle, colour):
    """A broad leaf seen flat: an ellipse `length` long and `width` wide at (cx, cy), its long axis at `angle` (radians)."""
    ca, sa = math.cos(angle), math.sin(angle)
    pts = []
    for k in range(14):
        a = 2.0 * math.pi * k / 14
        x, y = 0.5 * length * math.cos(a), 0.5 * width * math.sin(a)
        pts.append((cx + x * ca - y * sa, cy + x * sa + y * ca))
    d.polygon(pts, fill=colour + (255,))


def bush(rng, name, stems, leaf_len, leaf_w, greens, yellows, yellow_share, berries=0.0, arch=0.25,
         berry_colour=(32, 18, 30), berry_r=0.006, berry_count=(12, 22), berry_spread=(0.03, 0.012, 0.03), leafy_from=0.3,
         spread=0.42, leaf_mult=1.0):
    """A shrub in October (roadmap L7, A.3.23: the understorey and the brooks' bushes; L13, A.3.41: the scrub in the open),
    many stems from one foot fanning out, their upper part in leaves (from `leafy_from` of a stem's length): a hazel's broad
    round leaves turning yellow, an elder's narrower leaflets still green with black-purple umbels of berries; blackthorn's
    small dark leaves and blue-black sloes, hawthorn's lobed leaves turning and its red haws, dog rose's arching canes and
    orange-red hips. Each fruiting stem has one cluster of `berry_count` fruits within `berry_spread` (sideways, up, down,
    of the card) of a point near its tip. The stems fan out to `spread` of the card either side at full length; each
    carries `leaf_mult` times the leaves. Drawn as three crossed cards (tools/blender/build_cards.py)."""
    img = canvas()
    d = ImageDraw.Draw(img)
    W = H = SIZE * SS
    base_y = H - 4
    tips = []
    for i in range(stems):
        x0 = W * rng.uniform(0.42, 0.58)
        length = H * rng.uniform(0.55, 0.95)
        lean = W * (-spread + 2.0 * spread * (i + rng.uniform(0.2, 0.8)) / stems) * (length / H)  # spread across the fan, not by chance
        blade(d, x0, base_y, length, lean, W * rng.uniform(0.012, 0.022), (70, 60, 48), (104, 92, 74), taper=0.6)
        tips.append((x0, length, lean))
    # The leaves in layers: the inner ones darker (in the bush's shade), the outer lit; each along a stem's upper part.
    for layer in range(3):
        shade = 0.62 + 0.19 * layer
        for x0, length, lean in tips:
            for _ in range(int(rng.uniform(10, 18) * leaf_mult)):
                t = rng.uniform(leafy_from, 1.0)
                droop = arch * length * t * t * (1.0 - t) * 1.5
                x = x0 + lean * t * t + rng.uniform(-0.05, 0.05) * W
                y = base_y - length * t + droop + rng.uniform(-0.04, 0.04) * H
                base = rng.choice(yellows) if rng.random() < yellow_share else rng.choice(greens)
                colour = tuple(max(0, min(255, int(c * shade * rng.uniform(0.9, 1.08)))) for c in base)
                ellipse_leaf(d, x, y, leaf_len * W * rng.uniform(0.8, 1.2), leaf_w * W * rng.uniform(0.8, 1.2), rng.uniform(0.0, math.pi), colour)
    for x0, length, lean in tips:
        if rng.random() < berries:
            t = rng.uniform(0.6, 0.95)
            cx, cy = x0 + lean * t * t, base_y - length * t
            for _ in range(rng.randint(*berry_count)):
                r = W * berry_r
                bx, by = cx + rng.uniform(-berry_spread[0], berry_spread[0]) * W, cy + rng.uniform(-berry_spread[1], berry_spread[2]) * H
                d.ellipse((bx - r, by - r, bx + r, by + r), fill=berry_colour + (255,))
    fill = tuple(int(sum(c[i] for c in greens + yellows) / len(greens + yellows) * 0.8) for i in range(3))
    return finish(img, name, fill=fill)

TREE_W, TREE_H = 512, 1024  # a tree's card: its crown about half as wide as the tree is tall (map_dressing.json crown_ratio)


def tree_card(rng, name, shape, crown_base, leaves, leaf_len, leaf_w, greens, autumn, autumn_share, bark, clumps=0, open_share=0.0, lumps=0.18, top=False):
    """A whole tree on one card (roadmap L7, A.3.24: the kinds still drawn as primitives), our own drawing: a trunk from the
    foot to the crown, branches up into it, and leaves filling the crown's outline `shape(y)` (the half-width, 0..1 of the
    card's, at y 0 the crown's foot to 1 its top), darker inside and lit at its edge, in October's colours. `clumps` > 0
    gathers the leaves into that many clumps with gaps between (an open crown); drawn as three crossed cards."""
    W, H = TREE_W * SS, TREE_H * SS
    img = canvas(TREE_W, TREE_H)
    d = ImageDraw.Draw(img)
    foot_y = H - 4
    top_y = H * 0.01
    base_y = foot_y - (foot_y - top_y) * crown_base
    span = base_y - top_y
    cx = W * 0.5

    # The outline's lumps: a few waves up each side, its own on each, so the crown is never a smooth oval.
    waves = [[(rng.uniform(4.0, 11.0), rng.uniform(0.0, 2.0 * math.pi), rng.uniform(0.4, 1.0)) for _ in range(3)] for _ in range(2)]

    def half(t, side):
        bump = sum(a * math.sin(f * t * math.pi + ph) for f, ph, a in waves[side]) / 2.4
        return min(0.88, shape(t) * (1.0 + lumps * bump))  # inside its half of the atlas, leaf sprays and all

    def inside(x, y):
        t = (base_y - y) / span
        if t < 0.0 or t > 1.0:
            return False
        return abs(x - cx) <= half(t, 0 if x < cx else 1) * 0.5 * W

    # The trunk and the branches into the crown.
    trunk_w = W * 0.05
    blade(d, cx + W * rng.uniform(-0.01, 0.01), foot_y, (foot_y - top_y) * 0.92, W * rng.uniform(-0.02, 0.02), trunk_w, bark[0], bark[1], taper=0.8, segments=24)
    for _ in range(rng.randint(6, 10)):
        t = rng.uniform(0.0, 0.7)
        y0 = base_y - span * t
        side = rng.choice([-1, 1])
        reach = shape(min(1.0, t + 0.15)) * 0.5 * W * rng.uniform(0.6, 0.95)
        blade(d, cx, y0, span * rng.uniform(0.15, 0.35), side * reach, trunk_w * rng.uniform(0.25, 0.45), bark[0], bark[1], taper=0.85)
    centres = [(cx + rng.uniform(-0.35, 0.35) * W, base_y - span * rng.uniform(0.1, 0.9)) for _ in range(clumps)]
    centres = [(x, y) for x, y in centres if inside(x, y)]
    # The leaves in three layers, inner and shaded first, the lit rim last.
    placed = 0
    tries = 0
    while placed < leaves and tries < leaves * 30:
        tries += 1
        x, y = rng.uniform(0.0, W), rng.uniform(top_y, base_y)
        if not inside(x, y):
            continue
        if centres:
            near = min(math.hypot(x - a, y - b) for a, b in centres)
            if near > W * 0.13 and rng.random() < 0.92:
                continue
        if open_share > 0.0 and rng.random() < open_share:
            continue
        t = (base_y - y) / span
        rim = min(1.0, abs(x - cx) / max(1.0, half(t, 0 if x < cx else 1) * 0.5 * W))  # 0 at the middle, 1 at the outline
        lit = 0.55 + 0.35 * rim + 0.15 * t + rng.uniform(-0.06, 0.06)
        base = rng.choice(autumn) if rng.random() < autumn_share * (0.6 + 0.6 * rim) else rng.choice(greens)
        colour = tuple(max(0, min(255, int(c * lit))) for c in base)
        # A spray of leaves on a twig, not one leaf alone.
        for _ in range(rng.randint(2, 4)):
            ellipse_leaf(d, x + rng.uniform(-1.5, 1.5) * leaf_len * W, y + rng.uniform(-1.5, 1.5) * leaf_len * W, leaf_len * W * rng.uniform(0.8, 1.25),
                         leaf_w * W * rng.uniform(0.8, 1.25), rng.uniform(0.0, math.pi), colour)
        placed += 1
    fill = tuple(int(sum(c[i] for c in greens + autumn) / len(greens + autumn) * 0.7) for i in range(3))
    if not top:
        return finish(img, name, TREE_W, TREE_H, fill=fill)
    # The crown from above (top=True; tried in A.3.24, see build_cards.py TOPS), on the atlas's right half, its upper
    # square: the same leaves within a lumpy circle, lit more than the side, darker toward the middle.
    top = canvas(TREE_W, TREE_W)
    dt = ImageDraw.Draw(top)
    R = 0.40 * TREE_W * SS
    c0 = 0.5 * TREE_W * SS
    ring = [(rng.uniform(3.0, 7.0), rng.uniform(0.0, 2.0 * math.pi), rng.uniform(0.4, 1.0)) for _ in range(3)]
    for _ in range(int(leaves * 1.1)):
        a = rng.uniform(0.0, 2.0 * math.pi)
        edge = min(0.44 * TREE_W * SS, R * (1.0 + lumps * sum(k * math.sin(f * a + ph) for f, ph, k in ring) / 2.4))
        r = edge * math.sqrt(rng.random())
        if centres and rng.random() < 0.6:
            r *= rng.uniform(0.3, 1.0)  # an open crown's leaves gather in clumps
        x, y = c0 + r * math.cos(a), c0 + r * math.sin(a)
        rim = r / max(1.0, edge)
        lit = 0.62 + 0.38 * rim + rng.uniform(-0.06, 0.06)
        base = rng.choice(autumn) if rng.random() < autumn_share * (0.6 + 0.6 * rim) else rng.choice(greens)
        colour = tuple(max(0, min(255, int(ch * lit))) for ch in base)
        for _ in range(rng.randint(2, 4)):
            ellipse_leaf(dt, x + rng.uniform(-1.5, 1.5) * leaf_len * W, y + rng.uniform(-1.5, 1.5) * leaf_len * W, leaf_len * W * rng.uniform(0.8, 1.25),
                         leaf_w * W * rng.uniform(0.8, 1.25), rng.uniform(0.0, math.pi), colour)
    atlas = canvas(2 * TREE_W, TREE_H)
    atlas.paste(img, (0, 0))
    atlas.paste(top, (TREE_W * SS, 0))
    return finish(atlas, name, 2 * TREE_W, TREE_H, fill=fill)


def tree_cards():
    """The four kinds (A.3.24): Lombardy and hybrid poplar along the plain's roads and rows, a narrow column turning
    yellow; black alder by the water, an oval crown still dark green; black locust (acacia) in the rows and groves, a
    thin open crown of small leaflets, yellow-green; field maple, a round dense crown turning orange and yellow."""
    out = []
    out.append(tree_card(random.Random(301), "poplar_card", lambda t: 0.42 * math.sin(math.pi * min(1.0, t * 0.95 + 0.04)) ** 0.5 + 0.06 * (1.0 - t),
                         0.12, 1100, 0.035, 0.03, [(96, 112, 48), (110, 124, 54)], [(200, 178, 64), (214, 190, 80), (184, 160, 58)], 0.65,
                         ((70, 66, 58), (120, 116, 104))))
    out.append(tree_card(random.Random(311), "alder_card", lambda t: 0.92 * math.sin(math.pi * min(1.0, 0.1 + 0.9 * t)) ** 0.7 * (1.0 - 0.35 * t),
                         0.3, 1100, 0.04, 0.032, [(44, 66, 32), (54, 78, 36), (62, 86, 40)], [(120, 116, 52)], 0.08,
                         ((48, 42, 38), (86, 78, 70))))
    out.append(tree_card(random.Random(321), "acacia_card", lambda t: 0.98 * math.sin(math.pi * t) ** 0.45,
                         0.42, 1300, 0.024, 0.013, [(108, 124, 56), (122, 136, 62)], [(176, 170, 74), (196, 184, 86)], 0.45,
                         ((62, 54, 46), (104, 92, 78)), clumps=12, lumps=0.3))
    out.append(tree_card(random.Random(331), "maple_card", lambda t: 0.96 * math.sqrt(max(0.0, 1.0 - (2.0 * t - 1.0) ** 2)) ** 0.8,
                         0.33, 1200, 0.04, 0.036, [(96, 108, 46)], [(206, 120, 40), (222, 156, 52), (196, 168, 60), (178, 90, 36)], 0.8,
                         ((58, 52, 46), (100, 92, 82))))
    # Scots pine: a tall bare trunk, orange-brown up top, the crown a flat irregular cap of dark needle tufts in clumps.
    out.append(tree_card(random.Random(341), "pine_card", lambda t: 0.9 * math.sin(math.pi * min(1.0, 0.25 + 0.75 * t)) ** 0.35,
                         0.58, 1300, 0.028, 0.012, [(34, 58, 36), (42, 66, 40), (50, 74, 44)], [(70, 84, 48)], 0.1,
                         ((88, 60, 40), (168, 104, 60)), clumps=9, lumps=0.35))
    # Beech: a broad dome on a smooth grey trunk, the leaves turning copper and orange, still green inside.
    out.append(tree_card(random.Random(351), "beech_card", lambda t: 0.97 * math.sin(math.pi * min(1.0, 0.08 + 0.92 * t)) ** 0.6,
                         0.26, 1300, 0.036, 0.024, [(88, 100, 44), (100, 108, 48)], [(176, 96, 38), (194, 120, 46), (160, 84, 34), (204, 150, 60)], 0.75,
                         ((104, 104, 98), (150, 148, 140))))
    # White willow by a village pond and among the brooks' alders (A.3.37): a broad, loose dome of narrow leaves, silvery
    # grey-green, on a short furrowed trunk; it keeps its leaves late, a few gone yellow.
    out.append(tree_card(random.Random(361), "willow_card", lambda t: 0.97 * math.sin(math.pi * min(1.0, 0.05 + 0.95 * t)) ** 0.5,
                         0.22, 1500, 0.035, 0.011, [(132, 144, 112), (146, 158, 126), (116, 128, 96)], [(184, 176, 96)], 0.15,
                         ((70, 64, 56), (112, 104, 92)), clumps=10, lumps=0.25))
    return out


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
    # The woods' bushes (A.3.23): hazel turning yellow, elder still green with its berries.
    sheets += [bush(random.Random(211), "hazel_card", 11, 0.075, 0.062, [(92, 104, 44), (110, 116, 50)], [(178, 160, 62), (196, 172, 70), (160, 138, 56)], 0.55),
               bush(random.Random(223), "elder_card", 8, 0.06, 0.03, [(56, 78, 34), (68, 90, 40), (80, 98, 46)], [(150, 140, 60)], 0.12, berries=0.6, arch=0.45)]
    # The scrub in the open (L13, A.3.41): blackthorn dense and dark from the ground with blue-black sloes, hawthorn's small
    # leaves turning, with red haws all over, dog rose's arching canes yellowing, with orange-red hips.
    sheets += [bush(random.Random(307), "blackthorn_card", 18, 0.04, 0.02, [(34, 46, 26), (42, 54, 30), (50, 60, 34)], [(104, 96, 56), (88, 64, 52)], 0.12,
                    berries=0.8, arch=0.12, berry_colour=(46, 52, 78), berry_r=0.007, berry_count=(4, 9), berry_spread=(0.06, 0.08, 0.04), leafy_from=0.05,
                    spread=0.62, leaf_mult=2.6),
               bush(random.Random(311), "hawthorn_card", 13, 0.046, 0.038, [(58, 76, 34), (68, 86, 38), (52, 68, 30)], [(168, 122, 48), (150, 84, 38), (182, 146, 60)], 0.35,
                    berries=0.9, arch=0.2, berry_colour=(150, 26, 22), berry_r=0.0065, berry_count=(6, 12), berry_spread=(0.07, 0.07, 0.05), leafy_from=0.2,
                    spread=0.56, leaf_mult=1.8),
               bush(random.Random(313), "dog_rose_card", 12, 0.042, 0.026, [(76, 96, 44), (88, 106, 50)], [(182, 152, 60), (160, 120, 50)], 0.4,
                    berries=0.85, arch=0.65, berry_colour=(186, 54, 24), berry_r=0.008, berry_count=(2, 5), berry_spread=(0.05, 0.05, 0.05), leafy_from=0.1,
                    spread=0.58, leaf_mult=1.9)]
    trees = tree_cards()
    sheet = Image.new("RGB", (SIZE * len(sheets) + len(talls) * MAIZE_W // 4, SIZE), (92, 110, 150))
    for i, im in enumerate(sheets):
        sheet.paste(im.convert("RGB"), (i * SIZE, 0), mask=im.split()[3])
    for i, tall in enumerate(talls):
        small = tall.resize((MAIZE_W // 4, SIZE), Image.LANCZOS)
        sheet.paste(small.convert("RGB"), (SIZE * len(sheets) + i * MAIZE_W // 4, 0), mask=small.split()[3])
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
    if done.returncode != 0 or len(ok) != 24:
        print(done.stdout[-1200:], done.stderr[-1200:])
        raise SystemExit("Blender did not build the cards")
    return 0


if __name__ == "__main__":
    sys.exit(main())
