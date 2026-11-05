"""Makes a twig-card atlas for a card-based tree (tools/blender/card_tree.py) from a leaf photograph atlas.

    python tools/leaf_atlas.py hornbeam art/thirdparty/models/mp_Tree_Hornbeam/tex/T_Hornbeam_01_Foliage_CA.png OUT_DIR

The source is an RGBA sheet of single leaves on transparency (the Megaplants' foliage atlases: Fab Standard, per machine).
Each leaf is cut out by connected component, then three twig cards are composed from them: a curved stem with leaves
along it, alternate sides, turned and scaled by chance (seeded by the species name, so a run is repeatable). The
atlas (2 x 2 tiles of 1024 px) is the three twig cards and a bark tile; writes OUT_DIR/<species>_cards.png and a preview
on grey. Used for the card trees' leaves only: nothing here is redistributable, so it lives under art/thirdparty.
"""
import math
import random
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

Image.MAX_IMAGE_PIXELS = None
TILE = 1024


def components(mask):
    """Connected components (8-neighbour) of a boolean array by union-find over runs: label image and the count."""
    h, w = mask.shape
    parent = {}

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    labels = np.zeros((h, w), dtype=np.int32)
    nxt = 1
    for y in range(h):
        row = mask[y]
        x = 0
        while x < w:
            if not row[x]:
                x += 1
                continue
            x0 = x
            while x < w and row[x]:
                x += 1
            near = set()
            if y > 0:
                for v in labels[y - 1, max(0, x0 - 1):min(w, x + 1)]:
                    if v:
                        near.add(int(v))
            if not near:
                lab = nxt
                parent[lab] = lab
                nxt += 1
            else:
                lab = min(near)
                for o in near:
                    ra, rb = find(o), find(lab)
                    if ra != rb:
                        parent[max(ra, rb)] = min(ra, rb)
            labels[y, x0:x] = lab
    flat = np.vectorize(lambda v: find(int(v)) if v else 0)(np.unique(labels))
    remap = dict(zip(np.unique(labels).tolist(), flat.tolist()))
    labels = np.vectorize(lambda v: remap[int(v)])(labels) if len(remap) < 5000 else labels
    return labels


def cut_leaves(path, min_frac=0.0015):
    """The leaves of a sheet as RGBA crops, each turned so its petiole end is at the bottom and its tip up."""
    im = Image.open(path).convert("RGBA")
    small = im.resize((1024, 1024), Image.BILINEAR)
    mask = np.array(small)[..., 3] > 110
    labels = components(mask)
    scale = im.size[0] / 1024
    full = np.array(im)
    out = []
    for lab in np.unique(labels):
        if lab == 0:
            continue
        ys, xs = np.nonzero(labels == lab)
        if len(ys) < min_frac * 1024 * 1024:
            continue
        x0, x1, y0, y1 = int(xs.min() * scale) - 6, int((xs.max() + 1) * scale) + 6, int(ys.min() * scale) - 6, int((ys.max() + 1) * scale) + 6
        x0, y0 = max(0, x0), max(0, y0)
        crop = Image.fromarray(full[y0:y1, x0:x1].copy())
        comp = Image.fromarray(((labels[int(y0 / scale):int(y1 / scale) + 1, int(x0 / scale):int(x1 / scale) + 1] == lab) * 255).astype(np.uint8)).resize(crop.size, Image.BILINEAR).filter(ImageFilter.MaxFilter(5))
        a = np.minimum(np.array(crop)[..., 3], np.array(comp))
        arr = np.array(crop)
        arr[..., 3] = a
        crop = Image.fromarray(arr)
        # PCA axis, the narrow end is the petiole.
        yy, xx = np.nonzero(a > 110)
        pts = np.stack([xx - xx.mean(), yy - yy.mean()])
        w, v = np.linalg.eigh(np.cov(pts))
        axis = v[:, 1]
        u = axis @ pts
        lo, hi = np.percentile(u, [8, 92])
        narrow_lo = (u < lo + 0.15 * (hi - lo)).sum() < (u > hi - 0.15 * (hi - lo)).sum()
        base_dir = -axis if narrow_lo else axis  # points from the leaf's centre to its tip, reversed below
        tip = axis if narrow_lo else -axis  # tip direction in image coords (x right, y down)
        ang = math.degrees(math.atan2(tip[0], -tip[1]))  # degrees clockwise from up
        crop = crop.rotate(ang, resample=Image.BICUBIC, expand=True)
        bb = crop.getbbox()
        if bb:
            crop = crop.crop(bb)
        out.append(crop)
    return out


def twig(leaves, rng, kind):
    """One twig card: a curved stem from the bottom centre up, leaves along it."""
    img = Image.new("RGBA", (TILE, TILE), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    n = {0: 15, 1: 12, 2: 18}[kind]
    lean = rng.uniform(-0.18, 0.18)
    pts = []
    for i in range(41):
        t = i / 40
        pts.append((TILE * (0.5 + lean * t * t + 0.03 * math.sin(6 * t + kind)), TILE * (0.98 - 0.94 * t)))
    d.line(pts, fill=(70, 52, 34, 255), width=9)
    order = list(range(n))
    for k in order:
        t = 0.18 + 0.78 * k / max(1, n - 1)
        px, py = pts[min(40, int(t * 40))]
        side = rng.choice((-1, 1))
        lf = rng.choice(leaves)
        target_h = TILE * rng.uniform(0.15, 0.23) * (1.0 - 0.3 * t)
        s = target_h / lf.size[1]
        lf = lf.resize((max(2, int(lf.size[0] * s)), max(2, int(lf.size[1] * s))), Image.LANCZOS)
        ang = side * rng.uniform(25, 70)
        r = lf.rotate(-ang, resample=Image.BICUBIC, expand=True)
        # the leaf's base (bottom centre before turning) sits on the stem
        bx, by = lf.size[0] / 2, lf.size[1]
        cx, cy = lf.size[0] / 2, lf.size[1] / 2
        rad = math.radians(-ang)
        dx, dy = bx - cx, by - cy
        rx = dx * math.cos(rad) - dy * math.sin(rad)
        ry = dx * math.sin(rad) + dy * math.cos(rad)
        ox, oy = r.size[0] / 2 + rx, r.size[1] / 2 + ry
        img.alpha_composite(r, (int(px - ox), int(py - oy))) if 0 <= px - ox and 0 <= py - oy and px - ox + r.size[0] <= TILE and py - oy + r.size[1] <= TILE else _paste_clipped(img, r, int(px - ox), int(py - oy))
    # the top leaf, pointing up
    lf = rng.choice(leaves)
    s = TILE * 0.2 / lf.size[1]
    lf = lf.resize((max(2, int(lf.size[0] * s)), max(2, int(lf.size[1] * s))), Image.LANCZOS)
    _paste_clipped(img, lf, int(pts[-1][0] - lf.size[0] / 2), int(pts[-1][1] - lf.size[1] * 0.9))
    return img


def _paste_clipped(img, tile, x, y):
    layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
    layer.paste(tile, (x, y))
    img.alpha_composite(layer)


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        return 2
    species, src, out = sys.argv[1], sys.argv[2], Path(sys.argv[3])
    out.mkdir(parents=True, exist_ok=True)
    rng = random.Random(species)
    leaves = cut_leaves(src)
    print(f"{species}: {len(leaves)} leaves cut")
    atlas = Image.new("RGBA", (TILE * 2, TILE * 2), (0, 0, 0, 0))
    for k in range(3):
        atlas.alpha_composite(twig(leaves, rng, k), ((k % 2) * TILE, (k // 2) * TILE))
    bark = Image.new("RGBA", (TILE, TILE), (74, 58, 42, 255))
    noise = (np.random.RandomState(7).rand(TILE, TILE) * 22).astype(np.int16)
    arr = np.array(bark).astype(np.int16)
    arr[..., :3] = np.clip(arr[..., :3] + noise[..., None] - 11, 0, 255)
    atlas.paste(Image.fromarray(arr.astype(np.uint8)), (TILE, TILE))
    atlas.save(out / f"{species}_cards.png")
    bg = Image.new("RGBA", atlas.size, (130, 130, 130, 255))
    bg.alpha_composite(atlas)
    bg.convert("RGB").resize((1024, 1024), Image.LANCZOS).save(out / f"{species}_cards_preview.jpg", quality=90)
    return 0


if __name__ == "__main__":
    sys.exit(main())
