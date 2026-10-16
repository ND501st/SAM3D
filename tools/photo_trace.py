"""Trace a flat-fronted object (a radio, a control box, a console block) from photographs into millimetres, for a 3D model.

The steps of docs/MODELLING_FROM_PHOTOS.md that work on the photos; each reads the object's trace config
(tools/traces/<name>.json) and writes into out/traces/<name>/:

    python tools/photo_trace.py grid     CONFIG PHOTO                  # the photo with a labelled pixel grid, to pick edge windows
    python tools/photo_trace.py rectify  CONFIG PHOTO [--check]        # fit the in-plane edges, flatten: front_mm.png (+ lines)
    python tools/photo_trace.py tiles    CONFIG [--step 110]           # the flattened front cut in gridded mm tiles, to read off
    python tools/photo_trace.py circles  CONFIG NAME=X,Y,R ...         # RANSAC circle fits near guesses (mm), as JSON
    python tools/photo_trace.py camera   CONFIG PHOTO                  # solve the photo's camera from the plate: camera.json
    python tools/photo_trace.py ellipses CONFIG PHOTO2 NAME=PX,PY,R0,R1 ...  # round parts in a raw photo (px): their centres
    python tools/photo_trace.py align    CONFIG PHOTO2 NAME=PX,PY:X,Y ...   # map a second photo into the mm frame by matched points
    python tools/photo_trace.py overlay  CONFIG RENDER [--photo PHOTO] # a render on the flattened front (or on the photo): blend/edges

The config: {"name", "photo_size": [w, h], "height_mm" (the front's real height, from a cited source),
"edges": {"h_...": [x0, x1, y0, y1], "v_...": [y0, y1, x0, x1]} (windows round straight edges that lie in the front's plane:
h_ edges are horizontal on the object, v_ vertical), "plate_edges": {"top", "left", "bottom"} (px in the first flattened image,
read off its grid; set after the first rectify), "px_per_mm" (5), "margin_mm" (40)}.

Needs numpy and OpenCV (cv2), Pillow. The photos stay out of the repository (docs/SOURCES.md holds their links).
"""
import argparse
import json
import math
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def _cv():
    try:
        import cv2
        import numpy as np
    except ImportError:
        sys.exit('needs numpy and opencv-python')
    return cv2, np


def load(path):
    cfg = json.load(open(path, encoding='utf-8'))
    cfg.setdefault('px_per_mm', 5.0)
    cfg.setdefault('margin_mm', 40.0)
    cfg['out'] = os.path.join(REPO, 'out', 'traces', cfg['name'])
    os.makedirs(cfg['out'], exist_ok=True)
    return cfg


def read_photo(cfg, path):
    cv2, _ = _cv()
    im = cv2.imread(path)
    if im is None:
        sys.exit('cannot read ' + path)
    if 'photo_size' in cfg and list(im.shape[1::-1]) != list(cfg['photo_size']):
        sys.exit('expected a %s photo, got %s' % (cfg['photo_size'], list(im.shape[1::-1])))
    return im


def draw_grid(img, step, label_every, origin=(0.0, 0.0), scale=1.0, font_scale=0.5):
    """A grid every `step` units over img, where a unit is `scale` px and the image's (0, 0) is `origin` units."""
    cv2, _ = _cv()
    h, w = img.shape[:2]
    u0, v0 = origin
    u = math.ceil(u0 / step) * step
    while (u - u0) * scale < w:
        x = int((u - u0) * scale)
        major = abs(u / label_every - round(u / label_every)) < 1e-6
        cv2.line(img, (x, 0), (x, h), (0, 0, 255) if major else (0, 200, 255), 1)
        if major:
            cv2.putText(img, '%g' % u, (x + 2, 14), cv2.FONT_HERSHEY_SIMPLEX, font_scale, (255, 255, 0), 1)
        u += step
    v = math.ceil(v0 / step) * step
    while (v - v0) * scale < h:
        y = int((v - v0) * scale)
        major = abs(v / label_every - round(v / label_every)) < 1e-6
        cv2.line(img, (0, y), (w, y), (0, 0, 255) if major else (0, 200, 255), 1)
        if major:
            cv2.putText(img, '%g' % v, (2, y - 3), cv2.FONT_HERSHEY_SIMPLEX, font_scale, (255, 255, 0), 1)
        v += step
    return img


# ---------------------------------------------------------------------------------------------- rectification

def fit_line(gx, gy, axis, a0, a1, b0, b1, np):
    """The strongest gradient across a window, sample by sample, fitted as a line with RANSAC: (point, direction)."""
    pts = []
    for a in range(a0, a1, 4):
        if axis == 'h':
            col = np.abs(gy[b0:b1, a])
            pts.append((a, b0 + int(np.argmax(col))))
        else:
            row = np.abs(gx[a, b0:b1])
            pts.append((b0 + int(np.argmax(row)), a))
    P = np.array(pts, float)
    rng = np.random.RandomState(0)
    best = None
    for _ in range(400):
        i, j = rng.choice(len(P), 2, replace=False)
        d = P[j] - P[i]
        n = np.array([-d[1], d[0]])
        if np.linalg.norm(n) < 1e-9:
            continue
        n /= np.linalg.norm(n)
        inl = np.abs((P - P[i]) @ n) < 2.5
        if best is None or inl.sum() > best.sum():
            best = inl
    Q = P[best]
    c = Q.mean(0)
    _, _, vt = np.linalg.svd(Q - c)
    return c, vt[0], int(best.sum()), len(P)


def _homog(line, np):
    (x0, y0), (vx, vy) = line[0], line[1]
    h = np.cross([x0, y0, 1.0], [x0 + vx, y0 + vy, 1.0])
    return h / np.linalg.norm(h[:2])


def _vp(lines, np):
    _, _, vt = np.linalg.svd(np.array([_homog(l, np) for l in lines]))
    return vt[-1]


def fit_edges(cfg, im):
    cv2, np = _cv()
    g = cv2.GaussianBlur(cv2.cvtColor(im, cv2.COLOR_BGR2GRAY).astype(np.float32), (7, 7), 0)
    gx = cv2.Sobel(g, cv2.CV_32F, 1, 0, ksize=5)
    gy = cv2.Sobel(g, cv2.CV_32F, 0, 1, ksize=5)
    lines = {}
    for k, (a0, a1, b0, b1) in cfg['edges'].items():
        lines[k] = fit_line(gx, gy, 'h' if k.startswith('h') else 'v', a0, a1, b0, b1, np)
    return lines


def projective(lines):
    """The homography sending the horizontal and vertical vanishing points to infinity along x and y."""
    _, np = _cv()
    vh = _vp([l for k, l in lines.items() if k.startswith('h')], np)
    vv = _vp([l for k, l in lines.items() if k.startswith('v')], np)
    linf = np.cross(vh, vv)
    linf = linf / linf[2]
    P = np.array([[1, 0, 0], [0, 1, 0], linf])
    dh, dv = (P @ vh)[:2], (P @ vv)[:2]
    dh, dv = dh / np.linalg.norm(dh), dv / np.linalg.norm(dv)
    if dh[0] < 0:
        dh = -dh
    if dv[1] < 0:
        dv = -dv
    M = np.eye(3)
    M[:2, :2] = np.linalg.inv(np.array([[dh[0], dv[0]], [dh[1], dv[1]]]))
    return M @ P, vh, vv


def to_mm_homography(cfg, lines):
    """Photo px -> the mm frame's px (px_per_mm, margin): the projective step, then scale and offset from plate_edges."""
    _, np = _cv()
    H, vh, vv = projective(lines)
    pe = cfg.get('plate_edges')
    if not pe:
        return None, H, vh, vv
    k, m = cfg['px_per_mm'], cfg['margin_mm']
    s = cfg['height_mm'] / (pe['bottom'] - pe['top'])
    S = np.array([[s * k, 0, (-pe['left'] * s + m) * k], [0, s * k, (-pe['top'] * s + m) * k], [0, 0, 1.0]])
    return S @ H, H, vh, vv


def frame_size(cfg):
    k, m = cfg['px_per_mm'], cfg['margin_mm']
    return int((cfg['width_mm'] + 2 * m) * k), int((cfg['height_mm'] + 2 * m) * k)


def cmd_grid(cfg, args):
    cv2, _ = _cv()
    im = read_photo(cfg, args.photo)
    s = 1500.0 / im.shape[1]
    small = cv2.resize(im, None, fx=s, fy=s)
    draw_grid(small, 50, 250, scale=s)
    out = os.path.join(cfg['out'], 'photo_grid.jpg')
    cv2.imwrite(out, small)
    print('wrote', out, '(labels are full-size photo px)')


def cmd_rectify(cfg, args):
    cv2, np = _cv()
    im = read_photo(cfg, args.photo)
    lines = fit_edges(cfg, im)
    Hmm, H, vh, vv = to_mm_homography(cfg, lines)
    for k, (c, d, n_in, n) in lines.items():
        v = vh if k.startswith('h') else vv
        to = v[:2] / v[2] - c
        to /= np.linalg.norm(to)
        res = math.degrees(math.asin(min(1.0, abs(to[0] * d[1] - to[1] * d[0]))))
        flag = '' if res < 1.0 and n_in > 0.3 * n else '   <- check this window'
        print('  %-10s %3d/%3d samples on the line, %.2f deg off its vanishing point%s' % (k, n_in, n, res, flag))
    if Hmm is None:
        # first pass: flatten without scale, so the plate's edges can be read off a grid
        T = np.array([[1, 0, 500], [0, 1, 100], [0, 0, 1.0]])
        out = cv2.warpPerspective(im, T @ H, (int(im.shape[1] * 1.3), int(im.shape[0] * 1.3)), flags=cv2.INTER_CUBIC)
        draw_grid(out, 50, 250)
        path = os.path.join(cfg['out'], 'flat_unscaled.jpg')
        cv2.imwrite(path, out)
        print('wrote', path, ': read the plate\'s top, left and bottom edges off it (px, before the 500/100 offset is removed:'
              ' the labels already include it) into the config\'s plate_edges, with "offset": [500, 100], then rectify again')
        return
    if 'width_mm' not in cfg:
        pe = cfg['plate_edges']
        print('width: set width_mm from the flattened right edge (px %s known only after a look)' % pe)
    size = frame_size(cfg) if 'width_mm' in cfg else (int(2000), int(1500))
    out = cv2.warpPerspective(im, Hmm, size, flags=cv2.INTER_CUBIC)
    path = os.path.join(cfg['out'], 'front_mm.png')
    cv2.imwrite(path, out)
    print('wrote', path)
    k, m = cfg['px_per_mm'], cfg['margin_mm']
    g = draw_grid(out.copy(), 10, 50, origin=(-m, -m), scale=k)
    cv2.imwrite(os.path.join(cfg['out'], 'front_mm_grid.jpg'), g)
    json.dump({'H': Hmm.tolist()}, open(os.path.join(cfg['out'], 'homography.json'), 'w'))
    if args.check:
        chk = im.copy()
        for c, d, _, _ in lines.values():
            cv2.line(chk, tuple(int(v) for v in c - d * 4000), tuple(int(v) for v in c + d * 4000), (0, 0, 255), 2)
        cv2.imwrite(os.path.join(cfg['out'], 'rectify_lines.jpg'), cv2.resize(chk, None, fx=0.5, fy=0.5))


def _apply_offset(cfg):
    """plate_edges are read off flat_unscaled.jpg, which is the projective image moved by `offset`."""
    off = cfg.get('offset', [500, 100])
    pe = cfg['plate_edges']
    return {'top': pe['top'] - off[1], 'left': pe['left'] - off[0], 'bottom': pe['bottom'] - off[1]}


# ---------------------------------------------------------------------------------------------- reading the front

def front(cfg):
    cv2, _ = _cv()
    path = os.path.join(cfg['out'], 'front_mm.png')
    im = cv2.imread(path)
    if im is None:
        sys.exit('run rectify first (%s)' % path)
    return im


def cmd_tiles(cfg, args):
    cv2, _ = _cv()
    im = front(cfg)
    k, m = cfg['px_per_mm'], cfg['margin_mm']
    step = args.step
    n = 0
    y = -5.0
    while y < cfg['height_mm']:
        x = -5.0
        while x < cfg['width_mm']:
            c = im[int((y + m) * k):int((y + step + 15 + m) * k), int((x + m) * k):int((x + step + m) * k)].copy()
            draw_grid(c, 5, 10, origin=(x, y), scale=k, font_scale=0.4)
            c = cv2.resize(c, None, fx=1.3, fy=1.3)
            cv2.imwrite(os.path.join(cfg['out'], 'tile_%02d.png' % n), c)
            n += 1
            x += step
        y += step
    print('wrote %d tiles (x %g..%g mm each, 5 mm grid) in %s' % (n, step, step + 15, cfg['out']))


def fit_circle(edges_xy, cx, cy, r, np, lo=0.8, hi=1.2):
    xs, ys = edges_xy
    d = np.hypot(xs - cx, ys - cy)
    sel = (d > r * lo) & (d < r * hi)
    Q = np.stack([xs[sel], ys[sel]], 1).astype(np.float64)
    if len(Q) < 10:
        return None
    rng = np.random.RandomState(0)
    best = None
    for _ in range(2000):
        a, b, c = Q[rng.choice(len(Q), 3, replace=False)]
        A = np.array([[b[0] - a[0], b[1] - a[1]], [c[0] - a[0], c[1] - a[1]]]) * 2
        if abs(np.linalg.det(A)) < 1e-6:
            continue
        o = np.linalg.solve(A, np.array([b @ b - a @ a, c @ c - a @ a]))
        R = np.linalg.norm(a - o)
        if not (r * lo < R < r * hi) or np.hypot(*(o - [cx, cy])) > r * 0.3:
            continue
        inl = np.abs(np.hypot(Q[:, 0] - o[0], Q[:, 1] - o[1]) - R) < max(2.0, R * 0.015)
        if best is None or inl.sum() > best[0]:
            best = (inl.sum(), inl, R)
    if best is None:
        return None
    Qi = Q[best[1]]
    sol = np.linalg.lstsq(np.c_[2 * Qi, np.ones(len(Qi))], (Qi ** 2).sum(1), rcond=None)[0]
    o = sol[:2]
    R = math.sqrt(sol[2] + o @ o)
    return o[0], o[1], R, best[0] / (2 * math.pi * R)


def cmd_circles(cfg, args):
    cv2, np = _cv()
    im = front(cfg)
    k, m = cfg['px_per_mm'], cfg['margin_mm']
    e = cv2.Canny(cv2.GaussianBlur(cv2.cvtColor(im, cv2.COLOR_BGR2GRAY), (5, 5), 0), 35, 100)
    ys, xs = np.nonzero(e)
    out = {}
    draw = im.copy()
    for spec in args.guesses:
        name, vals = spec.split('=')
        x, y, r = (float(v) for v in vals.split(','))
        f = fit_circle((xs, ys), (x + m) * k, (y + m) * k, r * k, np)
        if f is None:
            print('%-12s no fit' % name)
            continue
        fx, fy, fr, cov = f[0] / k - m, f[1] / k - m, f[2] / k, f[3]
        out[name] = [round(fx, 1), round(fy, 1), round(fr, 2)]
        print('%-12s centre (%.1f, %.1f) r %.2f mm, %.0f %% of the rim seen%s' % (
            name, fx, fy, fr, 100 * min(cov, 1.0), '' if cov > 0.5 else '   <- weak: check by eye'))
        cv2.circle(draw, (int(f[0]), int(f[1])), int(f[2]), (0, 0, 255), 2)
    json.dump(out, open(os.path.join(cfg['out'], 'circles.json'), 'w'), indent=0)
    cv2.imwrite(os.path.join(cfg['out'], 'circles.jpg'), cv2.resize(draw, None, fx=0.8, fy=0.8))
    print('wrote circles.json and circles.jpg (a fit can catch a chamfer outside the opening: compare with the tiles)')


# ---------------------------------------------------------------------------------------------- cameras and overlays

def cmd_camera(cfg, args):
    """Solve the photo's camera (focal length, pose) from the plate: points of the mm frame and where the homography puts
    them in the photo. Writes camera.json, which tools/blender/trace_check.py turns into a Blender camera."""
    cv2, np = _cv()
    im = read_photo(cfg, args.photo)
    H = np.array(json.load(open(os.path.join(cfg['out'], 'homography.json')))['H'])
    Hi = np.linalg.inv(H)
    k, m = cfg['px_per_mm'], cfg['margin_mm']
    pts = [(x, y) for x in np.linspace(10, cfg['width_mm'] - 10, 12) for y in np.linspace(10, cfg['height_mm'] - 10, 8)]
    obj = np.array([[x / 1000, y / 1000, 0] for x, y in pts], np.float32)
    img = cv2.perspectiveTransform(np.array([[[(x + m) * k, (y + m) * k]] for x, y in pts], np.float64), Hi)
    img = img.reshape(-1, 2).astype(np.float32)
    w, h = im.shape[1], im.shape[0]
    K0 = np.array([[2.0 * w, 0, w / 2], [0, 2.0 * w, h / 2], [0, 0, 1]], np.float64)
    flags = (cv2.CALIB_USE_INTRINSIC_GUESS | cv2.CALIB_FIX_PRINCIPAL_POINT | cv2.CALIB_FIX_ASPECT_RATIO |
             cv2.CALIB_ZERO_TANGENT_DIST | cv2.CALIB_FIX_K1 | cv2.CALIB_FIX_K2 | cv2.CALIB_FIX_K3)
    err, K, _, rv, tv = cv2.calibrateCamera([obj], [img], (w, h), K0, np.zeros(5), flags=flags)
    R, _ = cv2.Rodrigues(rv[0])
    t = tv[0].ravel()
    C = -R.T @ t
    out = {'f_px': float(K[0, 0]), 'w': w, 'h': h, 'R': R.tolist(), 't': t.tolist(), 'rms_px': float(err),
           'frame': 'plate: x right, y down, z into the object, metres, origin at its top left corner'}
    json.dump(out, open(os.path.join(cfg['out'], 'camera.json'), 'w'), indent=1)
    print('rms %.2f px, f %.0f px; camera %.2f m right, %.2f m up, %.2f m out from the plate\'s top left' % (
        err, K[0, 0], C[0], -C[1], -C[2]))


def cmd_ellipses(cfg, args):
    """Round parts seen at an angle in a raw photo: ellipses fitted (RANSAC) to the edges between radii R0 and R1 px round a
    guess. Their centres are the matched points `align` needs (a circle's centre projects close to its ellipse's centre
    when the circle is small against the distance)."""
    cv2, np = _cv()
    im = cv2.imread(args.photo)
    e = cv2.Canny(cv2.GaussianBlur(cv2.cvtColor(im, cv2.COLOR_BGR2GRAY), (5, 5), 0), 40, 110)
    ys, xs = np.nonzero(e)
    draw = im.copy()
    for spec in args.guesses:
        name, vals = spec.split('=')
        cx, cy, r0, r1 = (float(v) for v in vals.split(','))
        d = np.hypot(xs - cx, ys - cy)
        P = np.stack([xs[(d > r0) & (d < r1)], ys[(d > r0) & (d < r1)]], 1).astype(np.float32)
        if len(P) < 12:
            print('%-10s too few edge points' % name)
            continue
        rng = np.random.RandomState(1)
        best = None
        for _ in range(1500):
            try:
                (ex, ey), (a, b), ang = cv2.fitEllipse(P[rng.choice(len(P), 6, replace=False)])
            except cv2.error:
                continue
            if not (a > 0 and b > 0):
                continue
            c, sn = math.cos(math.radians(ang)), math.sin(math.radians(ang))
            X = (P[:, 0] - ex) * c + (P[:, 1] - ey) * sn
            Y = -(P[:, 0] - ex) * sn + (P[:, 1] - ey) * c
            inl = np.abs(np.sqrt((X / (a / 2)) ** 2 + (Y / (b / 2)) ** 2) - 1) < 0.012
            if best is None or inl.sum() > best.sum():
                best = inl
        (ex, ey), (a, b), ang = cv2.fitEllipse(P[best])
        print('%-10s centre %.1f,%.1f px  axes %.0f x %.0f  (%d/%d edge points)' % (name, ex, ey, a, b, best.sum(), len(P)))
        cv2.ellipse(draw, ((ex, ey), (a, b), ang), (0, 0, 255), 3)
    cv2.imwrite(os.path.join(cfg['out'], 'ellipses.jpg'), cv2.resize(draw, None, fx=0.5, fy=0.5))


def cmd_align(cfg, args):
    """A second photo into the mm frame by matched points (photo2 px : mm), e.g. well centres fitted in both."""
    cv2, np = _cv()
    im = cv2.imread(args.photo)
    src, dst = [], []
    for spec in args.pairs:
        _, vals = spec.split('=')
        a, b = vals.split(':')
        src.append([float(v) for v in a.split(',')])
        dst.append([float(v) for v in b.split(',')])
    k, m = cfg['px_per_mm'], cfg['margin_mm']
    src = np.array(src, np.float32)
    dst_px = (np.array(dst, np.float32) + m) * k
    H, _ = cv2.findHomography(src, dst_px, 0)
    back = cv2.perspectiveTransform(src.reshape(-1, 1, 2), H).reshape(-1, 2) / k - m
    for (name, d, b) in zip(args.pairs, dst, back):
        print('  %-10s off by %.1f mm' % (name.split('=')[0], math.hypot(d[0] - b[0], d[1] - b[1])))
    out = cv2.warpPerspective(im, H, frame_size(cfg), flags=cv2.INTER_CUBIC)
    name = os.path.splitext(os.path.basename(args.photo))[0]
    cv2.imwrite(os.path.join(cfg['out'], 'second_%s_mm.png' % name), out)
    blend = cv2.addWeighted(front(cfg), 0.5, out, 0.5, 0)
    cv2.imwrite(os.path.join(cfg['out'], 'second_%s_blend.jpg' % name), cv2.resize(blend, None, fx=0.8, fy=0.8))
    print('wrote second_%s_mm.png and its blend with the front' % name)


def cmd_overlay(cfg, args):
    """A render on the flattened front (a front ortho render at px_per_mm, framed as front_mm.png: trace_check.py
    render_front), or on the photo itself (a render from camera.json: trace_check.py render_photo_camera), as a 50/50 blend
    and as the render's edges in red."""
    cv2, np = _cv()
    r = cv2.imread(args.render, cv2.IMREAD_UNCHANGED)
    base = cv2.imread(args.photo) if args.photo else front(cfg)
    base = cv2.resize(base, (r.shape[1], r.shape[0]))
    rgb = r[:, :, :3]
    a = (r[:, :, 3:4].astype(float) / 255.0) if r.shape[2] == 4 else np.ones(rgb.shape[:2] + (1,))
    blend = (base * (1 - 0.5 * a) + rgb * 0.5 * a).astype(np.uint8)
    edges = base.copy()
    edges[cv2.Canny(cv2.cvtColor(rgb, cv2.COLOR_BGR2GRAY), 40, 120) > 0] = (0, 0, 255)
    tag = 'photo' if args.photo else 'front'
    cv2.imwrite(os.path.join(cfg['out'], 'overlay_%s_blend.jpg' % tag), blend)
    cv2.imwrite(os.path.join(cfg['out'], 'overlay_%s_edges.jpg' % tag), edges)
    print('wrote overlay_%s_blend.jpg and overlay_%s_edges.jpg in %s' % (tag, tag, cfg['out']))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    sub = ap.add_subparsers(dest='cmd', required=True)
    for name in ('grid', 'rectify', 'camera'):
        p = sub.add_parser(name)
        p.add_argument('config')
        p.add_argument('photo')
        if name == 'rectify':
            p.add_argument('--check', action='store_true')
    p = sub.add_parser('tiles')
    p.add_argument('config')
    p.add_argument('--step', type=float, default=110.0)
    p = sub.add_parser('circles')
    p.add_argument('config')
    p.add_argument('guesses', nargs='+')
    p = sub.add_parser('ellipses')
    p.add_argument('config')
    p.add_argument('photo')
    p.add_argument('guesses', nargs='+')
    p = sub.add_parser('align')
    p.add_argument('config')
    p.add_argument('photo')
    p.add_argument('pairs', nargs='+')
    p = sub.add_parser('overlay')
    p.add_argument('config')
    p.add_argument('render')
    p.add_argument('--photo')
    args = ap.parse_args()
    cfg = load(args.config)
    if cfg.get('plate_edges'):
        cfg['plate_edges'] = _apply_offset(cfg)
    {'grid': cmd_grid, 'rectify': cmd_rectify, 'tiles': cmd_tiles, 'circles': cmd_circles, 'camera': cmd_camera,
     'align': cmd_align, 'ellipses': cmd_ellipses, 'overlay': cmd_overlay}[args.cmd](cfg, args)


if __name__ == '__main__':
    main()
