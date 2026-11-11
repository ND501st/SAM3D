"""A panel's face as a metric flat, and what is on it, measured by code.

A flat is one plane of a photo or of a book drawing, laid square at a known number of pixels per millimetre:

    plates   a rectangle in a photo: its four sides are picked edge chains (evidence.py's numbered tiles), a line fitted
             along each, the corners where the lines meet. Its proportions come from what is round on it (dials, knobs:
             the flat is stretched until they are circles), its size from a part of known size on it or a stated width.
    views    a panel of the cabin data seen through a certified camera (tools/traces/osa_interior/cameras.json): the
             photo laid on the panel's own plane, so the flat's millimetres are the data's.

On a flat, SAM 2.1's automatic masks are numbered and measured in millimetres (centre, width, height, roundness,
colour). Whoever reads the flat answers with numbers only: which mask is which part. A part no mask found is asked for
with a point prompt (which thing is meant, never a measurement). The picks are kept in
tools/traces/osa_interior/panels/<flat>.json with what was measured; `panels.py apply` writes them into the cabin data.

    panel_flats.py flat <id>                      # the flat: out/recon/osa/panels/flats/<id>.png (+ _quad.jpg, the sides on the photo)
    panel_flats.py parts <id> [--side 48]         # numbered masks: <id>_parts.jpg, <id>_parts.json
    panel_flats.py show <id> [--box x0,y0,x1,y1 mm] [--zoom 2] [--grid 10]   # a piece of the flat with a millimetre grid
    panel_flats.py prompt <id> name x,y [x,y ...] [--neg x,y]              # one more part by points on the flat (mm)
    panel_flats.py picked <id>                    # the picks drawn back on the flat

Run with E:/SAM3D-tools/venv-depth/Scripts/python.exe -X utf8.
"""
import argparse
import os
import sys

import cv2
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import core as C  # noqa: E402

SPECS = os.path.join(C.TRACES, 'panel_flats.json')
PICKS = os.path.join(C.TRACES, 'panels')
OUT = os.path.join(C.OUT, 'panels', 'flats')


def specs():
    return C.load(SPECS)['flats']


def spec(fid):
    for s in specs():
        if s['id'] == fid:
            return s
    raise KeyError('no flat %s in %s' % (fid, SPECS))


def source(s):
    """The picture a flat is cut from: a photo of the book run, or another flat (a plate inside a plane already laid flat)."""
    if 'of_flat' in s:
        return cv2.imread(os.path.join(OUT, s['of_flat'] + '.png'))
    return C.photo(s['photo'], s.get('scan', 'clean'))


# ------------------------------------------------------------------------------------------------ the rectangle
def quad_of(s):
    """The four corners (TL, TR, BR, BL) of the plate's rectangle in its source picture, from the picked sides."""
    import evidence as E
    import cameras as K
    t = s['tile']
    img = source(s)
    x0, y0, x1, y1 = t['box']
    ch = [p + [x0, y0] for p, _ in E.chains_of(cv2.cvtColor(img[y0:y1, x0:x1], cv2.COLOR_BGR2GRAY), t.get('sigma', 2.5), t.get('min', 70))]
    lines, info = {}, {}
    for side in ('top', 'right', 'bottom', 'left'):
        if side not in s['edges']:
            continue
        P = np.vstack([K.side_points(ch, pick, side) for pick in s['edges'][side]])
        lines[side] = K.fit_line(P)
        c, v = lines[side]
        info[side] = {'points': int(len(P)), 'rms_px': round(float(np.sqrt(np.mean(((P - c) @ np.array([-v[1], v[0]])) ** 2))), 2)}
    if 'mirror' in s:
        # a side cut off in the picture (a book drawing cropped at its edge): the opposite side mirrored about the plate's
        # middle, which is halfway between pairs of parts drawn symmetrically (the handles); an upright drawing only
        m = s['mirror']
        mid = np.mean([0.5 * (np.vstack([K.side_points(ch, a, 'any')]).mean(0)[0] + np.vstack([K.side_points(ch, b, 'any')]).mean(0)[0]) for a, b in m['pairs']])
        c, v = lines[m['from']]
        lines[m['side']] = (np.array([2 * mid - c[0], c[1]]), np.array([-v[0], v[1]]))
        info[m['side']] = {'mirrored_from': m['from'], 'middle_px': round(float(mid), 1)}
    q = np.array([K.meet(lines['top'], lines['left']), K.meet(lines['top'], lines['right']), K.meet(lines['bottom'], lines['right']), K.meet(lines['bottom'], lines['left'])])
    return q, lines, info


def _provisional(q):
    return 0.5 * (np.linalg.norm(q[1] - q[0]) + np.linalg.norm(q[2] - q[3])), 0.5 * (np.linalg.norm(q[3] - q[0]) + np.linalg.norm(q[2] - q[1]))


def _warp(img, q, W, H, ext, ppm):
    """The picture laid flat: the quad is the rectangle W x H mm, the flat reaches ext = (left, top, right, bottom) mm beyond it."""
    l, t, r, b = ext
    dst = np.array([[l, t], [l + W, t], [l + W, t + H], [l, t + H]], np.float32) * ppm
    M = cv2.getPerspectiveTransform(q.astype(np.float32), dst)
    size = (int(round((l + W + r) * ppm)), int(round((t + H + b) * ppm)))
    return cv2.warpPerspective(img, M, size, flags=cv2.INTER_CUBIC), M


def round_parts(flat, lo_px, hi_px):
    """Round things on a flat: ellipses fitted to SAM's masks, (cx, cy, width, height) in px, for the ones an ellipse fits well."""
    ms = auto_masks(flat, 32)
    out = []
    for m in ms:
        cs, _ = cv2.findContours(m.astype(np.uint8), cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)
        c = max(cs, key=cv2.contourArea)
        if len(c) < 30:
            continue
        (cx, cy), (a, b), ang = cv2.fitEllipse(c)
        if not (lo_px <= max(a, b) <= hi_px) or min(a, b) < 0.6 * max(a, b):
            continue
        fill = cv2.contourArea(c) / (np.pi * a * b / 4)
        if not 0.93 < fill < 1.05:
            continue
        # the ellipse's extent along the flat's own axes
        th = np.radians(ang)
        w = np.hypot(a * np.cos(th), b * np.sin(th))
        h = np.hypot(a * np.sin(th), b * np.cos(th))
        out.append((cx, cy, w, h))
    return out


def register(img, par, pim, box_mm=None, ratio=0.75):
    """The map from a flat's pixels to a photo's (a homography), from SIFT matches between them on the plane's parts that
    both show (box_mm on the flat: u0, v0, u1, v1). Returns H, the inlier count and their rms in photo px."""
    sift = cv2.SIFT_create(nfeatures=20000)
    g1 = cv2.cvtColor(pim, cv2.COLOR_BGR2GRAY)
    mask = None
    if box_mm:
        u0, v0, u1, v1 = box_mm
        (x0, y0), (x1, y1) = to_px(par, [[u0, v1], [u1, v0]]).astype(int)
        mask = np.zeros(g1.shape, np.uint8)
        mask[max(y0, 0):y1, max(x0, 0):x1] = 255
    k1, d1 = sift.detectAndCompute(g1, mask)
    k2, d2 = sift.detectAndCompute(cv2.cvtColor(img, cv2.COLOR_BGR2GRAY), None)
    good = [m for m, n in cv2.BFMatcher().knnMatch(d1, d2, k=2) if m.distance < ratio * n.distance]
    A = np.float32([k1[m.queryIdx].pt for m in good])
    B = np.float32([k2[m.trainIdx].pt for m in good])
    H, inl = cv2.findHomography(A, B, cv2.RANSAC, 3.0)
    inl = inl.ravel().astype(bool)
    P = cv2.perspectiveTransform(A[inl][None], H)[0]
    return H, int(inl.sum()), round(float(np.sqrt(np.mean(np.sum((P - B[inl]) ** 2, 1)))), 2)


def build(fid, quiet=False):
    """Lay the flat and write it with its numbers: the quad, the map from the source's pixels to the flat's, mm per pixel."""
    s = spec(fid)
    os.makedirs(OUT, exist_ok=True)
    img = source(s)
    ppm = float(s.get('px_per_mm', 3.0))
    doc = {'id': fid}
    if 'match_flat' in s:                              # another photo of a plane already laid flat, in that flat's millimetres
        par, pim = meta(s['match_flat']), image(s['match_flat'])
        Hpf, n_in, rms = register(img, par, pim, s.get('match_box_mm'))
        u0, v0, u1, v1 = s['box_mm']
        k = par['px_per_mm'] / ppm
        A = np.array([[k, 0, (u0 + par['origin_mm'][0]) * par['px_per_mm']], [0, k, (par['origin_mm'][1] - v1) * par['px_per_mm']], [0, 0, 1]])
        M = np.linalg.inv(Hpf @ A)
        size = (int(round((u1 - u0) * ppm)), int(round((v1 - v0) * ppm)))
        flat = cv2.warpPerspective(img, M, size, flags=cv2.INTER_CUBIC)
        q = cv2.perspectiveTransform(np.float32([[0, 0], [size[0], 0], [size[0], size[1]], [0, size[1]]])[None], np.linalg.inv(M))[0]
        doc.update(size_mm=[u1 - u0, v1 - v0], origin_mm=[-u0, v1], matches=n_in, match_rms_px=rms,
                   how='%s registered on the flat %s by %d SIFT matches (rms %.2f px): the same plane, in its millimetres' % (s['photo'], s['match_flat'], n_in, rms))
    elif 'view' in s:                                  # a panel of the cabin data through a certified camera
        import panels as P
        v = s['view']
        pan = {p['id']: p for p in P.cabin()['panels']}[v['panel']]
        if 'centre_m' in v:
            pan = dict(pan, **{k: v[k] for k in ('centre_m', 'facing_deg', 'tilt_deg', 'size_m') if k in v})
        w, h = pan['size_m']
        l, t, r, b = [e / 1000.0 for e in s.get('extend_mm', [0, 0, 0, 0])]
        uv = np.array([[-w / 2 - l, h / 2 + t], [w / 2 + r, h / 2 + t], [w / 2 + r, -h / 2 - b], [-w / 2 - l, -h / 2 - b]])
        q = C.project(C.camera(s['photo']), P.body(pan, uv))
        W, H = (w + l + r) * 1000, (h + t + b) * 1000
        flat, M = _warp(img, q, W, H, (0, 0, 0, 0), ppm)
        doc.update(size_mm=[w * 1000, h * 1000], origin_mm=[(w / 2 + l) * 1000, (h / 2 + t) * 1000], panel=v['panel'], how='the cabin data\'s panel through the certified camera of %s' % s['photo'])
    else:
        q, lines, info = quad_of(s)
        ext = s.get('extend_mm', [0, 0, 0, 0])
        W0, H0 = _provisional(q)
        if 'size_mm' in s:
            W, H = s['size_mm']
            how = 'size as stated'
        elif 'of_flat' in s:                           # a plate inside a plane already laid flat: its millimetres are that flat's
            par = meta(s['of_flat'])
            W, H = W0 / par['px_per_mm'], H0 / par['px_per_mm']
            how = 'a plate of the flat %s, in its millimetres' % s['of_flat']
            doc['in_parent_mm'] = [round(float(v), 2) for v in to_mm(par, [q.mean(0)])[0]]
        else:
            # proportions: what is round on the plate must come out round
            prov, _ = _warp(img, q, W0, H0, (0, 0, 0, 0), 1.0)
            rp = round_parts(prov, float(s.get('round_min', 0.05)) * W0, 0.5 * min(W0, H0))      # dials and big knobs, not screws
            rp = [r for r in rp if 0.75 < r[2] / r[3] < 1.33]
            if s.get('aspect_from_sides'):
                k = 1.0
                how = 'a drawing: proportions as drawn'
            elif 'aspect' in s:
                k = W0 / H0 / float(s['aspect'])
                how = 'proportions as stated (%s)' % s['aspect']
            elif len(rp) >= 3:
                k = float(np.median([w / h for _, _, w, h in rp]))          # the flat is k times too wide for its height
                how = 'proportions from %d round parts (their width to height on the first flat: %.3f, spread %.3f)' % (len(rp), k, float(np.std([w / h for _, _, w, h in rp])))
                doc['round_parts_first_flat'] = [[round(float(v), 1) for v in r] for r in rp]
            else:
                k = 1.0
                how = 'proportions as the sides measure in the picture (fewer than three round parts found)'
            W = float(s['width_mm'])
            H = W * (H0 * k) / W0
        flat, M = _warp(img, q, W, H, ext, ppm)
        doc.update(size_mm=[round(W, 2), round(H, 2)], origin_mm=[ext[0] + W / 2, ext[1] + H / 2], sides=info, how=how)
        can = img.copy()
        for side, (c, v) in lines.items():
            cv2.line(can, tuple(int(k) for k in c - 4000 * v), tuple(int(k) for k in c + 4000 * v), (0, 0, 255), 1, cv2.LINE_AA)
        cv2.polylines(can, [np.round(q).astype(np.int32)], True, (0, 255, 255), 2, cv2.LINE_AA)
        x0, y0 = np.maximum(q.min(0) - 60, 0).astype(int)
        x1, y1 = (q.max(0) + 60).astype(int)
        cv2.imwrite(os.path.join(OUT, fid + '_quad.jpg'), can[y0:y1, x0:x1], [cv2.IMWRITE_JPEG_QUALITY, 90])
    doc.update(quad_px=np.round(q, 2).tolist(), M=M.tolist(), px_per_mm=ppm, flat_px=[flat.shape[1], flat.shape[0]],
               source=s.get('of_flat') or s['photo'], scale_basis=s.get('scale_basis', '') or (s.get('match_flat') and 'that of the flat %s' % s['match_flat']) or '')
    cv2.imwrite(os.path.join(OUT, fid + '.png'), flat)
    C.save(os.path.join(OUT, fid + '.json'), doc)
    if not quiet:
        print('%s: %s, %.1f x %.1f mm at %.1f px/mm -> %dx%d px; %s' % (fid, doc['source'], doc['size_mm'][0], doc['size_mm'][1], ppm, flat.shape[1], flat.shape[0], doc['how']))
        if 'sides' in doc:
            print('   sides', doc['sides'])
    return doc


def meta(fid):
    return C.load(os.path.join(OUT, fid + '.json'))


def image(fid):
    return cv2.imread(os.path.join(OUT, fid + '.png'))


def to_mm(m, px):
    """Flat pixels -> panel millimetres: u right, v up, from the plate's centre."""
    px = np.atleast_2d(np.asarray(px, float))
    return np.c_[px[:, 0] / m['px_per_mm'] - m['origin_mm'][0], m['origin_mm'][1] - px[:, 1] / m['px_per_mm']]


def to_px(m, mm):
    mm = np.atleast_2d(np.asarray(mm, float))
    return np.c_[(mm[:, 0] + m['origin_mm'][0]) * m['px_per_mm'], (m['origin_mm'][1] - mm[:, 1]) * m['px_per_mm']]


# ------------------------------------------------------------------------------------------------ parts
def auto_masks(img, side=48, min_area=60):
    import torch
    import evidence as E
    from sam2.automatic_mask_generator import SAM2AutomaticMaskGenerator
    gen = SAM2AutomaticMaskGenerator(E.sam(), points_per_side=side, points_per_batch=64, pred_iou_thresh=0.7, stability_score_thresh=0.88, box_nms_thresh=0.6, min_mask_region_area=min_area)
    with torch.inference_mode(), torch.autocast('cuda', dtype=torch.bfloat16):
        res = gen.generate(cv2.cvtColor(img, cv2.COLOR_BGR2RGB))
    return [r['segmentation'] for r in res]


COLOUR_NAMES = {'black': (25, 25, 25), 'white': (225, 225, 220), 'grey': (140, 145, 145), 'red': (170, 40, 35), 'amber': (225, 150, 40), 'yellow': (230, 200, 60),
                'green': (40, 120, 60), 'blue': (60, 90, 150), 'orange': (220, 110, 50), 'brass': (190, 160, 90), 'chrome': (190, 195, 195)}


def colour_name(rgb):
    return min(COLOUR_NAMES, key=lambda k: sum((a - b) ** 2 for a, b in zip(COLOUR_NAMES[k], rgb)))


def measure_mask(m, img, me):
    """What a mask is, in the flat's millimetres: centre, width and height along the plate, how round, its mean colour."""
    cs, _ = cv2.findContours(m.astype(np.uint8), cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)
    c = max(cs, key=cv2.contourArea)
    area = float(cv2.contourArea(c))
    x, y, w, h = cv2.boundingRect(c)
    ppm = me['px_per_mm']
    d = dict(area_mm2=round(area / ppm ** 2, 1), box_mm=[round(w / ppm, 1), round(h / ppm, 1)])
    cx, cy = x + w / 2.0, y + h / 2.0
    rnd = 0.0
    if len(c) >= 20:
        (ex, ey), (a, b), ang = cv2.fitEllipse(c)
        if max(a, b) > 0 and abs(ex - cx) < w and abs(ey - cy) < h:
            fill = area / (np.pi * a * b / 4)
            rnd = min(a, b) / max(a, b) * (1 - min(1, abs(fill - 1) * 4))
            if rnd > 0.8:
                cx, cy = ex, ey
                d['dia_mm'] = round(0.5 * (a + b) / ppm, 1)
    u, v = to_mm(me, [[cx, cy]])[0]
    bgr = img[m].mean(0)
    rgb = [int(bgr[2]), int(bgr[1]), int(bgr[0])]
    d.update(at_mm=[round(float(u), 1), round(float(v), 1)], round=round(float(rnd), 2), rgb=rgb, colour=colour_name(rgb))
    return d, c


def parts(fid, side=48, lo_mm=3.0, hi_frac=0.45):
    """Number the flat's masks in reading order and measure each."""
    me, img = meta(fid), image(fid)
    ms = auto_masks(img, side)
    ppm = me['px_per_mm']
    rows = []
    for m in ms:
        d, c = measure_mask(m, img, me)
        if max(d['box_mm']) < lo_mm or d['box_mm'][0] > hi_frac * img.shape[1] / ppm and d['box_mm'][1] > hi_frac * img.shape[0] / ppm:
            continue
        rows.append((d, c, m))
    # reading order: bands of 12 mm from the top, left to right in each
    rows.sort(key=lambda r: (-round(r[0]['at_mm'][1] / 12.0), r[0]['at_mm'][0]))
    keep = []
    for d, c, m in rows:                               # one mask per thing: drop a mask that all but repeats an earlier one
        if any(abs(d['at_mm'][0] - k[0]['at_mm'][0]) < 1.5 and abs(d['at_mm'][1] - k[0]['at_mm'][1]) < 1.5 and abs(d['box_mm'][0] - k[0]['box_mm'][0]) < 2 and abs(d['box_mm'][1] - k[0]['box_mm'][1]) < 2 for k in keep):
            continue
        keep.append((d, c, m))
    lab = np.zeros(img.shape[:2], np.int32)
    out = []
    for i, (d, c, m) in enumerate(sorted(keep, key=lambda r: -r[0]['area_mm2'])):
        pass
    for i, (d, c, m) in enumerate(keep):
        d['n'] = i
        out.append(d)
    np.savez_compressed(os.path.join(OUT, fid + '_parts.npz'), contours=np.array([k[1].reshape(-1, 2) for k in keep], dtype=object), allow_pickle=True)
    C.save(os.path.join(OUT, fid + '_parts.json'), {'flat': fid, 'side': side, 'parts': out})
    draw_parts(fid)
    print('%s: %d parts -> %s' % (fid, len(out), os.path.join(OUT, fid + '_parts.jpg')))
    return out


def _label(img, text, at, col=(0, 0, 0), scale=0.42):
    cv2.putText(img, text, at, 0, scale, (255, 255, 255), 3, cv2.LINE_AA)
    cv2.putText(img, text, at, 0, scale, col, 1, cv2.LINE_AA)


def draw_parts(fid, zoom=None, box=None, suffix='_parts'):
    me, img = meta(fid), image(fid)
    doc = C.load(os.path.join(OUT, fid + '_parts.json'))
    cons = np.load(os.path.join(OUT, fid + '_parts.npz'), allow_pickle=True)['contours']
    zoom = zoom or max(1.0, 1500.0 / img.shape[1])
    can = cv2.resize(img, None, fx=zoom, fy=zoom, interpolation=cv2.INTER_CUBIC)
    can = cv2.addWeighted(can, 0.75, np.full_like(can, 255), 0.25, 0)
    rng = np.random.default_rng(3)
    for d, c in zip(doc['parts'], cons):
        col = [int(v) for v in rng.integers(0, 200, 3)]
        cv2.polylines(can, [np.round(c * zoom).astype(np.int32)], True, col, 1, cv2.LINE_AA)
    for d, c in zip(doc['parts'], cons):
        p = to_px(me, [d['at_mm']])[0] * zoom
        _label(can, str(d['n']), (int(p[0]) - 6, int(p[1]) + 4))
    if box:
        (x0, y0), (x1, y1) = (to_px(me, [[box[0], box[3]]])[0] * zoom).astype(int), (to_px(me, [[box[2], box[1]]])[0] * zoom).astype(int)
        can = can[max(0, y0):y1, max(0, x0):x1]
    out = os.path.join(OUT, fid + suffix + '.jpg')
    cv2.imwrite(out, can, [cv2.IMWRITE_JPEG_QUALITY, 90])
    return out


def show(fid, box=None, zoom=2.0, grid=10.0, suffix='_show', picks=False):
    """A piece of the flat (box in mm: u0, v0, u1, v1) with a millimetre grid, for reading labels and for placing a prompt."""
    me, img = meta(fid), image(fid)
    W, H = img.shape[1] / me['px_per_mm'], img.shape[0] / me['px_per_mm']
    ox, oy = me['origin_mm']
    box = box or [-ox, oy - H, W - ox, oy]
    (x0, y1), (x1, y0) = to_px(me, [[box[0], box[1]]])[0], to_px(me, [[box[2], box[3]]])[0]
    x0, y0, x1, y1 = [int(round(k)) for k in (max(0, x0), max(0, y0), min(img.shape[1], x1), min(img.shape[0], y1))]
    sub = cv2.resize(img[y0:y1, x0:x1], None, fx=zoom, fy=zoom, interpolation=cv2.INTER_CUBIC)
    if grid:
        g = grid
        for u in np.arange(np.ceil(box[0] / g) * g, box[2], g):
            x = int((to_px(me, [[u, 0]])[0][0] - x0) * zoom)
            big = abs(u / g) % 5 < 1e-6
            cv2.line(sub, (x, 0), (x, sub.shape[0]), (255, 200, 0) if big else (255, 230, 150), 1)
            if big:
                _label(sub, '%g' % u, (x + 2, 11), (160, 60, 0), 0.38)
        for v in np.arange(np.ceil(box[1] / g) * g, box[3], g):
            y = int((to_px(me, [[0, v]])[0][1] - y0) * zoom)
            big = abs(v / g) % 5 < 1e-6
            cv2.line(sub, (0, y), (sub.shape[1], y), (255, 200, 0) if big else (255, 230, 150), 1)
            if big:
                _label(sub, '%g' % v, (2, y - 2), (160, 60, 0), 0.38)
    if picks:
        for it in load_picks(fid).get('items', []):
            draw_pick(sub, it, lambda mm: (to_px(me, mm) - [x0, y0]) * zoom)
    out = os.path.join(OUT, fid + suffix + '.jpg')
    cv2.imwrite(out, sub, [cv2.IMWRITE_JPEG_QUALITY, 90])
    print(out, sub.shape[:2])
    return out


# ------------------------------------------------------------------------------------------------ picks
def picks_file(fid):
    return os.path.join(PICKS, fid + '.json')


def load_picks(fid):
    p = picks_file(fid)
    return C.load(p) if os.path.exists(p) else {'flat': fid, 'items': []}


def prompt(fid, pos_mm, neg_mm=()):
    """One part by point prompts on the flat (mm): which thing is meant. Its measure comes from the mask."""
    import torch
    import evidence as E
    from sam2.sam2_image_predictor import SAM2ImagePredictor
    me, img = meta(fid), image(fid)
    key = ('flat', fid)
    if key not in E._SAM:
        p = SAM2ImagePredictor(E.sam())
        with torch.inference_mode(), torch.autocast('cuda', dtype=torch.bfloat16):
            p.set_image(cv2.cvtColor(img, cv2.COLOR_BGR2RGB))
        E._SAM[key] = p
    pts = np.vstack([to_px(me, pos_mm)] + ([to_px(me, neg_mm)] if len(neg_mm) else []))
    lab = np.array([1] * len(pos_mm) + [0] * len(neg_mm))
    with torch.inference_mode(), torch.autocast('cuda', dtype=torch.bfloat16):
        m, sc, _ = E._SAM[key].predict(point_coords=pts, point_labels=lab, multimask_output=True)
    return [(m[i].astype(bool), float(sc[i])) for i in range(len(sc))]


def draw_pick(img, it, f, col=None):
    col = col or {'control': (0, 160, 255), 'lamp': (40, 40, 255), 'display': (255, 140, 0)}.get(it.get('as', 'fixture'), (40, 200, 40))
    u, v = it['at_mm']
    w, h = it['size_mm'] if isinstance(it['size_mm'], list) else (it['size_mm'], it['size_mm'])
    if not isinstance(it['size_mm'], list):
        a = np.linspace(0, 2 * np.pi, 40)
        p = np.c_[u + w / 2 * np.cos(a), v + h / 2 * np.sin(a)]
    else:
        p = np.array([[u - w / 2, v - h / 2], [u + w / 2, v - h / 2], [u + w / 2, v + h / 2], [u - w / 2, v + h / 2]])
    cv2.polylines(img, [np.round(f(p)).astype(np.int32)], True, col, 1, cv2.LINE_AA)
    c = f([[u, v]])[0]
    _label(img, it.get('name', it.get('kind', ''))[:16], (int(c[0]) - 10, int(c[1]) - 4), col, 0.36)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('stage')
    ap.add_argument('id')
    ap.add_argument('rest', nargs='*')
    ap.add_argument('--side', type=int, default=48)
    ap.add_argument('--box')
    ap.add_argument('--zoom', type=float, default=2.0)
    ap.add_argument('--grid', type=float, default=10.0)
    ap.add_argument('--neg', action='append', default=[])
    ap.add_argument('--picks', action='store_true')
    a = ap.parse_args()
    box = [float(v) for v in a.box.split(',')] if a.box else None
    if a.stage == 'flat':
        build(a.id)
    elif a.stage == 'parts':
        parts(a.id, a.side)
    elif a.stage == 'show':
        show(a.id, box, a.zoom, a.grid, picks=a.picks)
    elif a.stage == 'zoomparts':
        print(draw_parts(a.id, a.zoom, box, '_parts_zoom'))
    elif a.stage == 'prompt':
        me, img = meta(a.id), image(a.id)
        pos = [[float(v) for v in k.split(',')] for k in a.rest[1:]]
        neg = [[float(v) for v in k.split(',')] for k in a.neg]
        for m, sc in prompt(a.id, pos, neg):
            d, _ = measure_mask(m, img, me)
            print(a.rest[0], 'score %.2f' % sc, d)
    else:
        sys.exit('unknown stage ' + a.stage)


if __name__ == '__main__':
    main()
