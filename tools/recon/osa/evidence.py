"""Stage 3 of docs/OSA_MODELLING_PROCESS.md: evidence in 2-D, by part and feature.

Two numbered pictures of a tile of a photo, made by code:

    masks    SAM 2.1's automatic masks: parts that differ in tone from what is round them
    chains   every edge as a chain placed on the gradient's peak to a fraction of a pixel: creases and rims of pressings

Whoever reads the photo answers with numbers only (which chains, which mask, are which part's which feature). Those
picks are kept in tools/traces/osa_interior/evidence/<photo>.json and `measure` turns them into outlines in the photo's
own pixels, kept beside them. No coordinate is read off a picture by eye; a point prompt only says which thing is meant.

    python tools/recon/osa/evidence.py tile g098-a 350,950,1150,1950 [--sigma 2.5] [--min 70] [--masks] [--zoom 1.0]
    python tools/recon/osa/evidence.py measure g098-a          # picks -> outlines, written back into the evidence file
    python tools/recon/osa/evidence.py draw g098-a [part ...]  # the outlines on the photo: out/recon/osa/evidence/<photo>[_part].jpg
"""
import argparse
import os
import sys

import cv2
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import core as C  # noqa: E402

TILES = os.path.join(C.OUT, 'tiles')
SAM_CKPT = r'E:\SAM3D-tools\models\sam2.1_hiera_base_plus.pt'
SAM_CFG = 'configs/sam2.1/sam2.1_hiera_b+.yaml'
N8 = [(-1, -1), (-1, 0), (-1, 1), (0, -1), (0, 1), (1, -1), (1, 0), (1, 1)]
STEP_PX = 3.0             # outlines are kept at about this spacing


# ------------------------------------------------------------------------------------------------ edge chains
def link(edge):
    """Thin edge pixels as chains: walked from end points, each pixel once."""
    e = edge.copy()
    h, w = e.shape
    nb = cv2.filter2D(e.astype(np.uint8), -1, np.ones((3, 3), np.float32), borderType=cv2.BORDER_CONSTANT) - e.astype(np.uint8)
    chains = []

    def walk(y, x):
        pts = [(x, y)]
        e[y, x] = False
        while True:
            nxt = None
            for dy, dx in N8:
                yy, xx = y + dy, x + dx
                if 0 <= yy < h and 0 <= xx < w and e[yy, xx]:
                    nxt = (yy, xx)
                    if dy == 0 or dx == 0:      # straight neighbours first, so a diagonal step does not skip a pixel
                        break
            if nxt is None:
                return pts
            y, x = nxt
            e[y, x] = False
            pts.append((x, y))                  # Canny's edges are thin but stepped: a pixel with three neighbours is usually a stair, not a junction
    ys, xs = np.nonzero(e & (nb == 1))
    for y, x in zip(ys, xs):
        if e[y, x]:
            chains.append(walk(y, x))
    ys, xs = np.nonzero(e)
    for y, x in zip(ys, xs):
        if e[y, x]:
            chains.append(walk(y, x))
    return chains


def chains_of(gray, sigma=2.5, min_len=70):
    """Edge chains of a grey tile, longest first: [(N x 2 sub-pixel points x, y; mean gradient)]. The same tile and settings always give the same numbering."""
    g = cv2.GaussianBlur(gray.astype(np.float32), (0, 0), sigma)
    gx, gy = cv2.Sobel(g, cv2.CV_32F, 1, 0, ksize=3), cv2.Sobel(g, cv2.CV_32F, 0, 1, ksize=3)
    mag = np.hypot(gx, gy)
    hi = float(np.percentile(mag, 80))
    lo = 0.35 * hi
    q = 16.0                                # Canny takes whole numbers: keep the weak gradients of a blurred print
    e = cv2.Canny(np.clip(gx * q, -32000, 32000).astype(np.int16), np.clip(gy * q, -32000, 32000).astype(np.int16), lo * q, hi * q, L2gradient=True) > 0
    out = []
    for c in link(e):
        if len(c) < min_len:
            continue
        p = np.array(c, float)
        xi, yi = p[:, 0].astype(int), p[:, 1].astype(int)
        m0 = mag[yi, xi]
        nx, ny = gx[yi, xi] / np.maximum(m0, 1e-6), gy[yi, xi] / np.maximum(m0, 1e-6)
        mp = cv2.remap(mag, (p[:, 0] + nx).astype(np.float32), (p[:, 1] + ny).astype(np.float32), cv2.INTER_LINEAR).ravel()
        mm = cv2.remap(mag, (p[:, 0] - nx).astype(np.float32), (p[:, 1] - ny).astype(np.float32), cv2.INTER_LINEAR).ravel()
        den = mp - 2 * m0 + mm
        off = np.where(np.abs(den) > 1e-6, 0.5 * (mm - mp) / den, 0.0).clip(-1, 1)      # the gradient's peak across the edge, by a parabola
        p[:, 0] += off * nx
        p[:, 1] += off * ny
        k = max(3, int(2 * sigma) | 1)
        ps = np.stack([np.convolve(np.pad(p[:, i], k // 2, mode='edge'), np.ones(k) / k, mode='valid') for i in (0, 1)], 1)
        out.append((ps, float(m0.mean())))
    out.sort(key=lambda t: (-len(t[0]), t[0][0][0], t[0][0][1]))
    return out


def tile_chains(photo, box, sigma=2.5, min_len=70):
    img = C.photo(photo)
    x0, y0, x1, y1 = box
    ch = chains_of(cv2.cvtColor(img[y0:y1, x0:x1], cv2.COLOR_BGR2GRAY), sigma, min_len)
    return [p + [x0, y0] for p, _ in ch]


# ------------------------------------------------------------------------------------------------ masks
_SAM = {}


def sam():
    if 'm' not in _SAM:
        from sam2.build_sam import build_sam2
        _SAM['m'] = build_sam2(SAM_CFG, SAM_CKPT, device='cuda')
    return _SAM['m']


def tile_masks(photo, box, side=32):
    """SAM 2.1's automatic masks of a tile, largest first, as full-photo boolean masks' crops with the tile's offset."""
    import torch
    from sam2.automatic_mask_generator import SAM2AutomaticMaskGenerator
    img = C.photo(photo)
    x0, y0, x1, y1 = box
    gen = SAM2AutomaticMaskGenerator(sam(), points_per_side=side, points_per_batch=32, pred_iou_thresh=0.7, stability_score_thresh=0.9, box_nms_thresh=0.7, min_mask_region_area=300)
    with torch.inference_mode(), torch.autocast('cuda', dtype=torch.bfloat16):
        res = gen.generate(cv2.cvtColor(img[y0:y1, x0:x1], cv2.COLOR_BGR2RGB))
    res.sort(key=lambda r: (-r['area'], r['bbox'][0], r['bbox'][1]))
    return [r['segmentation'] for r in res], [float(r['predicted_iou']) for r in res]


def prompt_mask(photo, pos, neg=(), box=None):
    """One mask by point prompts (the photo's pixels): which thing is meant, never a measurement."""
    import torch
    from sam2.sam2_image_predictor import SAM2ImagePredictor
    key = ('pred', photo)
    if key not in _SAM:
        p = SAM2ImagePredictor(sam())
        with torch.inference_mode(), torch.autocast('cuda', dtype=torch.bfloat16):
            p.set_image(cv2.cvtColor(C.photo(photo), cv2.COLOR_BGR2RGB))
        _SAM[key] = p
    pts = [list(q) for q in pos] + [list(q) for q in neg]
    lab = [1] * len(pos) + [0] * len(neg)
    with torch.inference_mode(), torch.autocast('cuda', dtype=torch.bfloat16):
        m, s, _ = _SAM[key].predict(box=None if box is None else np.asarray(box, float)[None, :], point_coords=np.asarray(pts, float), point_labels=np.asarray(lab), multimask_output=len(pts) <= 1 and box is None)
    i = int(np.argmax(s))
    mask = m[i].astype(bool)
    n, lab_im = cv2.connectedComponents(mask.astype(np.uint8))
    if n > 2:
        k = lab_im[int(pos[0][1]), int(pos[0][0])]
        mask = lab_im == (k if k > 0 else 1 + int(np.argmax(np.bincount(lab_im[lab_im > 0])[1:])))
    return mask, float(s[i])


def mask_outline(mask, offset=(0, 0)):
    """The mask's outer boundary as a closed outline, its own ragged pixels smoothed away."""
    m = cv2.morphologyEx(mask.astype(np.uint8), cv2.MORPH_OPEN, np.ones((5, 5), np.uint8))
    cs, _ = cv2.findContours(m, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)
    c = max(cs, key=cv2.contourArea).reshape(-1, 2).astype(float)
    k = 7
    c = np.stack([np.convolve(np.concatenate([c[-(k // 2):, i], c[:, i], c[:k // 2, i]]), np.ones(k) / k, mode='valid') for i in (0, 1)], 1)
    return c + list(offset)


# ------------------------------------------------------------------------------------------------ outlines from picks
def resample(p, step=STEP_PX):
    d = np.concatenate([[0], np.cumsum(np.linalg.norm(np.diff(p, axis=0), axis=1))])
    if d[-1] < step:
        return p
    t = np.arange(0, d[-1], step)
    return np.stack([np.interp(t, d, p[:, 0]), np.interp(t, d, p[:, 1])], 1)


def ordered(pieces, overlap_px=8.0):
    """Picked pieces as one line without going over the same stretch twice: each is laid on after the last, turned round if that is the nearer way, less the part that runs along what is already there (the two edges of one highlight)."""
    pieces = [np.asarray(c, float) for c in pieces]
    out = pieces[0]
    if len(pieces) > 1:
        c = pieces[1]
        if min(np.linalg.norm(out[0] - c[0]), np.linalg.norm(out[0] - c[-1])) < min(np.linalg.norm(out[-1] - c[0]), np.linalg.norm(out[-1] - c[-1])):
            out = out[::-1]
    for c in pieces[1:]:
        if np.linalg.norm(out[-1] - c[0]) > np.linalg.norm(out[-1] - c[-1]):
            c = c[::-1]
        d = np.array([np.min(np.linalg.norm(out - q, axis=1)) for q in c])
        new = np.flatnonzero(d > overlap_px)
        if len(new):
            out = np.vstack([out, c[new[0]:]])
    return out


def clip(p, part):
    """A stretch of a line, as shares of its length: [0.2, 1.0] drops the first fifth."""
    a, b = part
    n = len(p)
    return p[int(round(a * (n - 1))):int(round(b * (n - 1))) + 1]


def measure_item(photo, item, cache):
    kind = item['kind']
    if kind == 'chains':
        t = item['tile']
        key = ('c', tuple(t['box']), t.get('sigma', 2.5), t.get('min', 70))
        if key not in cache:
            cache[key] = tile_chains(photo, t['box'], t.get('sigma', 2.5), t.get('min', 70))
        picks = []
        for n in item['chains']:
            if isinstance(n, dict) and 'straight' in n:        # the k-th longest straight piece of a chain that turns a corner
                from cameras import straight_pieces
                picks.append(straight_pieces(cache[key][n['n']])[n['straight']])
            elif isinstance(n, dict):
                picks.append(clip(cache[key][n['n']], n['part']))
            else:
                picks.append(cache[key][n])
        return [resample(q) for q in picks], False
    if kind == 'mask':
        if 'auto' in item:
            t = item['auto']
            key = ('m', tuple(t['box']), t.get('side', 32))
            if key not in cache:
                cache[key] = tile_masks(photo, t['box'], t.get('side', 32))
            m = cache[key][0][t['n']]
            for extra in t.get('plus', []):
                m = m | cache[key][0][extra]
            return [resample(mask_outline(m, t['box'][:2]))], True
        m, s = prompt_mask(photo, item['prompt']['pos'], item['prompt'].get('neg', ()), item['prompt'].get('box'))
        item['score'] = round(s, 3)
        return [resample(mask_outline(m))], True
    raise ValueError('an evidence item is chains or mask: ' + kind)


def measure(photo):
    path = os.path.join(C.EVIDENCE, photo + '.json')
    doc = C.load(path)
    cache = {}
    for it in doc['items']:
        pieces, closed = measure_item(photo, it, cache)
        it['closed'] = closed
        it.pop('xy', None)
        it['pieces'] = [np.round(q, 1).tolist() for q in pieces]
        print('%-7s %-28s %-16s %d pieces, %4d points%s' % (photo, it['part'], it['feature'], len(pieces), sum(len(q) for q in pieces), '  score %.2f' % it['score'] if 'score' in it else ''))
    h, w = C.photo(photo).shape[:2]
    doc['size_px'] = [w, h]
    C.save(path, doc, indent=None)
    return doc


def items(photo, part=None, feature=None):
    path = os.path.join(C.EVIDENCE, photo + '.json')
    if not os.path.exists(path):
        return []
    out = []
    for it in C.load(path)['items']:
        if 'pieces' in it and (part is None or it['part'] == part) and (feature is None or it['feature'] == feature):
            pieces = [np.array(q, float) for q in it['pieces']]
            out.append(dict(it, pieces=pieces, xy=np.vstack(pieces), line=ordered(pieces)))
    return out


def views_of(part):
    return sorted(f[:-5] for f in os.listdir(C.EVIDENCE) if f.endswith('.json') and items(f[:-5], part))


# ------------------------------------------------------------------------------------------------ pictures
def _label(img, text, at, col=(0, 0, 0), scale=0.5):
    cv2.putText(img, text, at, 0, scale, (255, 255, 255), 3, cv2.LINE_AA)
    cv2.putText(img, text, at, 0, scale, col, 1, cv2.LINE_AA)


def tile(photo, box, sigma, min_len, masks, zoom):
    img = C.photo(photo)
    x0, y0, x1, y1 = box
    sub = img[y0:y1, x0:x1]
    os.makedirs(TILES, exist_ok=True)
    stem = os.path.join(TILES, '%s_%d_%d_%d_%d' % (photo, x0, y0, x1, y1))
    plain = cv2.resize(sub, None, fx=zoom, fy=zoom, interpolation=cv2.INTER_CUBIC) if zoom != 1 else sub
    cv2.imwrite(stem + '.jpg', plain, [cv2.IMWRITE_JPEG_QUALITY, 93])
    ch = tile_chains(photo, box, sigma, min_len)
    canvas = cv2.addWeighted(plain, 0.7, np.full_like(plain, 255), 0.3, 0)
    rng = np.random.default_rng(1)
    for p in ch:
        cv2.polylines(canvas, [np.round((p - [x0, y0]) * zoom).astype(np.int32)], False, [int(v) for v in rng.integers(0, 190, 3)], 2, cv2.LINE_AA)
    for i, p in enumerate(ch):
        _label(canvas, str(i), tuple(int(v) for v in (p[len(p) // 2] - [x0, y0]) * zoom))
    cv2.imwrite(stem + '_chains.jpg', canvas, [cv2.IMWRITE_JPEG_QUALITY, 92])
    print('%d chains (sigma %.1f, min %d) -> %s_chains.jpg %s' % (len(ch), sigma, min_len, stem, canvas.shape[:2]))
    if masks:
        ms, sc = tile_masks(photo, box)
        canvas = plain.copy()
        for i, m in enumerate(ms):
            col = [int(v) for v in rng.integers(60, 255, 3)]
            cs, _ = cv2.findContours(m.astype(np.uint8), cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)
            cv2.drawContours(canvas, [np.round(c * zoom).astype(np.int32) for c in cs], -1, col, 2)
        for i, m in enumerate(ms):
            ys, xs = np.nonzero(m)
            j = np.argmin((xs - np.median(xs)) ** 2 + (ys - np.median(ys)) ** 2)
            _label(canvas, str(i), (int(xs[j] * zoom), int(ys[j] * zoom)), scale=0.55)
        cv2.imwrite(stem + '_masks.jpg', canvas, [cv2.IMWRITE_JPEG_QUALITY, 92])
        print('%d masks -> %s_masks.jpg' % (len(ms), stem))


def draw(photo, parts=None, box=None, out=None):
    img = C.photo(photo)
    rng = np.random.default_rng(2)
    for it in items(photo):
        if parts and it['part'] not in parts:
            continue
        col = [int(v) for v in rng.integers(0, 255, 3)]
        cv2.polylines(img, [np.round(q).astype(np.int32) for q in it['pieces']], bool(it.get('closed')), col, 2, cv2.LINE_AA)
        _label(img, '%s.%s' % (it['part'], it['feature']), tuple(int(v) for v in it['xy'][len(it['xy']) // 3]), col)
    if box:
        img = img[box[1]:box[3], box[0]:box[2]]
    out = out or os.path.join(C.OUT, 'evidence', photo + ('_' + '_'.join(parts) if parts else '') + '.jpg')
    os.makedirs(os.path.dirname(out), exist_ok=True)
    cv2.imwrite(out, img, [cv2.IMWRITE_JPEG_QUALITY, 92])
    print(out, img.shape[:2])


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('stage', choices=['tile', 'measure', 'draw'])
    ap.add_argument('photo')
    ap.add_argument('rest', nargs='*')
    ap.add_argument('--sigma', type=float, default=2.5)
    ap.add_argument('--min', type=int, default=70)
    ap.add_argument('--masks', action='store_true')
    ap.add_argument('--zoom', type=float, default=1.0)
    ap.add_argument('--box')
    a = ap.parse_args()
    if a.stage == 'tile':
        tile(a.photo, [int(v) for v in a.rest[0].split(',')], a.sigma, a.min, a.masks, a.zoom)
    elif a.stage == 'measure':
        measure(a.photo)
    else:
        draw(a.photo, a.rest or None, [int(v) for v in a.box.split(',')] if a.box else None)


if __name__ == '__main__':
    main()
