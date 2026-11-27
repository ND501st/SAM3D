"""Stage 1 of docs/OSA_MODELLING_PROCESS.md: cameras proven on control.

The joint solve of the book's interior photos (tools/recon/holistic_sfm.py, as it was before holistic_refine.py) is
adjusted again with the control faces of tools/traces/osa_interior/control.json:

    frame   a face traced in millimetres whose place in the cabin defines the frame (SP-2M2): its points are fixed
    plate   any other flat face: its shape is known, its place, turn, scale (and aspect, where the flat was made from
            a rectangle of unknown proportions) are solved together with the cameras

Each flat is matched into every view (ALIKED + LightGlue, a homography keeping the matches that lie on the face).
COLMAP's bundle adjustment then moves cameras, focal lengths, lens terms and tie points with the control held; the
plates are re-fitted through the new cameras; and again, until the plates stop moving. Each view gets a certificate:

    A   a control face in view within the limit: may measure and judge
    B   solved by tie points only: may show
    C   not solved, or square-on to one plane: flats only

    python tools/recon/osa/cameras.py flats        # plates that are a rectangle in a photo: flattened through their picked sides
    python tools/recon/osa/cameras.py match        # flats into views (kept in out/recon/osa/control_matches.npz)
    python tools/recon/osa/cameras.py solve        # the adjustment, the certificates: tools/traces/osa_interior/cameras.json
    python tools/recon/osa/cameras.py show [photo] # every control face drawn on its photos: out/recon/osa/cameras/<photo>.jpg
"""
import os
import sys

import cv2
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import core as C  # noqa: E402
from core import V  # noqa: E402

MODEL = os.path.join(C.REPO, 'out', 'recon', 'holistic', 'sfm', 'model', 'models', '0')
ALIGN = os.path.join(C.REPO, 'out', 'recon', 'holistic', 'sfm', 'cameras.json')
MATCHES = os.path.join(C.OUT, 'control_matches.npz')
FLATS = os.path.join(C.OUT, 'flats')
MIN_ON_FACE = 40
MIN_FOR_CONTROL = 60     # fewer matches than this on a face do not hold a camera (g093-a had 40 on SP-2M2, and they were wrong)
PER_FACE = 300            # control points a face gives one view
A_LIMIT_PX = 3.0          # grade A: the view's control within this (median)
TIE_LIMIT_PX = 1.5
PP_BOUND = 0.20           # a principal point further from the centre than this share of the long side is not believed
ROUNDS = 3


# ------------------------------------------------------------------------------------------------ the control faces
class Face:
    """A control face: flat pixels -> plate coordinates (a right, b up, metres, from its centre) -> the body frame."""

    def __init__(self, d):
        self.d, self.id, self.kind = d, d['id'], d['kind']
        self.free = d.get('free', [])
        self.source = d.get('from_photo')
        if self.source:                           # a plate made from a rectangle in a photo (flats stage): the flat's margins are what it was extended by
            w, h = d['size_mm']
            ext = d.get('extend', [0, 0, 0, 0])
            self.ppm, self.margin = d.get('px_per_mm', 2.0), np.array([ext[0] * w, ext[1] * h])
            self.flat = os.path.join(FLATS, d['id'] + '.png')
            self.flat_size = (int(round((1 + ext[0] + ext[2]) * w * self.ppm)), int(round((1 + ext[1] + ext[3]) * h * self.ppm)))
        else:
            self.ppm, self.margin = d['px_per_mm'], d['margin_mm']
            self.flat = os.path.join(C.REPO, d['flat'])
        if 'panel' in d:
            p = V.load_cabin()['panels'][d['panel']]
            self.n, self.r, self.up = V.panel_axes(p)
            self.c = np.array(p['centre_m'], float)
        else:                                     # no place given: found from its own photo's camera when the solve starts (start_from)
            self.c, self.r, self.up, self.n = np.zeros(3), np.array([1.0, 0, 0]), np.array([0, 0, 1.0]), np.array([0, -1.0, 0])
            self.placed = False
        self.q = np.zeros(8)                      # a plate's move from its start: rotation vector, shift, log scale, log aspect
        if d.get('map') == 'cabinet0096':
            import cabinet0096_trace as T
            door, s = T.DOOR, T.DOOR_PROUD / -T.CAM_A[2]

            def ab(m):       # a reading on g096-a's flat of something on the door: back on the door (the trace's `fix`), about the door's centre, at the game's scale
                x, y = m[:, 0] * (1 - s) + T.CAM_A[0] * s, m[:, 1] * (1 - s) + T.CAM_A[1] * s
                return (x - 0.5 * (door[0] + door[2])) * 0.001 * T.SCALE, (0.5 * (door[1] + door[3]) - y) * 0.001 * T.SCALE
            self._ab, self.rect = ab, (28.0, 10.0, 653.5, 680.0)
        else:
            w, h = d['size_mm']
            self._ab, self.rect = (lambda m: ((m[:, 0] - w / 2) * 0.001, (h / 2 - m[:, 1]) * 0.001)), (0.0, 0.0, w, h)

    def ab(self, uv):
        return self._ab(np.atleast_2d(np.asarray(uv, float)) / self.ppm - self.margin)

    def body(self, uv, q=None):
        a, b = self.ab(uv)
        return self.place(a, b, self.q if q is None else q)

    def place(self, a, b, q):
        R = cv2.Rodrigues(np.asarray(q[:3], float))[0]
        s, k = np.exp(q[6]), np.exp(0.5 * q[7])
        local = s * (np.outer(a * k, self.r) + np.outer(b / k, self.up))
        return self.c + (R @ local.T).T + q[3:6]

    def corners_flat(self):
        x0, y0, x1, y1 = self.rect
        return (np.array([[x0, y0], [x1, y0], [x1, y1], [x0, y1]], float) + self.margin) * self.ppm

    def outline_body(self, n=40):
        c = self.corners_flat()
        return self.body(np.vstack([np.linspace(c[i], c[(i + 1) % 4], n) for i in range(4)]))

    def start_from(self, cam, flat_px, px):
        """A plate with no place given: where its own photo's camera puts it (a planar pose from its matches there)."""
        a, b_ = self.ab(flat_px)
        K = np.array([[cam.f, 0, cam.cx], [0, cam.f, cam.cy], [0, 0, 1.0]])
        ok, rv, tv = cv2.solvePnP(np.column_stack([a, b_, np.zeros(len(a))]), px.astype(np.float64), K, np.array([cam.k1, 0, 0, 0.0]), flags=cv2.SOLVEPNP_IPPE)
        Rp = cv2.Rodrigues(rv)[0]
        self.c = cam.R.T @ (tv.ravel() - cam.t)
        self.r, self.up = cam.R.T @ Rp[:, 0], cam.R.T @ Rp[:, 1]
        self.n = np.cross(self.r, self.up)
        self.placed = True

    def restore(self, doc):
        if 'start' in doc:
            self.c, self.r, self.up = (np.array(doc['start'][k], float) for k in ('centre_m', 'right', 'up'))
            self.n = np.cross(self.r, self.up)
        self.q = np.array(doc['q'], float)

    def summary(self):
        R = cv2.Rodrigues(self.q[:3])[0]
        return {'kind': self.kind, 'start': {'centre_m': self.c.round(5).tolist(), 'right': self.r.round(6).tolist(), 'up': self.up.round(6).tolist()}, 'centre_m': (self.c + self.q[3:6]).round(4).tolist(), 'normal': (R @ self.n).round(4).tolist(), 'right': (R @ self.r).round(4).tolist(), 'up': (R @ self.up).round(4).tolist(),
                'scale': round(float(np.exp(self.q[6])), 4), 'aspect': round(float(np.exp(self.q[7])), 4), 'moved_m': self.q[3:6].round(4).tolist(), 'turned_deg': round(float(np.degrees(np.linalg.norm(self.q[:3]))), 2), 'q': self.q.round(6).tolist()}


def faces():
    return {d['id']: Face(d) for d in C.load(C.CONTROL)['faces']}


def skip():
    return tuple(C.load(C.CONTROL).get('face_photos_only', []))


# ------------------------------------------------------------------------------------------------ flats from a rectangle in a photo
def straight_pieces(p, tol=1.5, min_pts=25):
    """A chain's straight pieces, longest first: lines found one after another, each taking the points within tol px of it."""
    p = np.asarray(p, float)
    left = np.ones(len(p), bool)
    rng = np.random.default_rng(0)
    out = []
    while left.sum() >= min_pts and len(out) < 6:
        idx = np.flatnonzero(left)
        best = None
        for _ in range(300):
            i, j = rng.choice(idx, 2, replace=False)
            d = p[j] - p[i]
            L = np.linalg.norm(d)
            if L < 20:
                continue
            n = np.array([-d[1], d[0]]) / L
            k = left & (np.abs((p - p[i]) @ n) < tol)
            if best is None or k.sum() > best.sum():
                best = k
        if best is None or best.sum() < min_pts:
            break
        out.append(p[best])
        left &= ~best
    return out


def side_points(chains, pick, side):
    """The points a pick names: a whole chain's longest straight piece, or the piece of it that is its top, bottom, left or right side in the picture."""
    if not isinstance(pick, dict):
        return straight_pieces(chains[pick])[0]
    want = pick.get('side', side)
    cand = []
    for q in straight_pieces(chains[pick['n']]):
        vt = np.linalg.svd(q - q.mean(0), full_matrices=False)[2][0]
        if (abs(vt[0]) > abs(vt[1])) == (want in ('top', 'bottom')):
            cand.append(q)
    if not cand:
        raise ValueError('chain %s has no %s side' % (pick['n'], want))
    cand = [q for q in cand if len(q) >= 0.4 * max(len(k) for k in cand)]         # a short stretch at a corner is not a side
    key = (lambda q: q[:, 1].mean()) if want in ('top', 'bottom') else (lambda q: q[:, 0].mean())
    return (min if want in ('top', 'left') else max)(cand, key=key)


def fit_line(P):
    """A line through points: a point on it and its direction."""
    c = P.mean(0)
    return c, np.linalg.svd(P - c, full_matrices=False)[2][0]


def meet(l1, l2):
    (p, d), (q, e) = l1, l2
    t = np.linalg.solve(np.column_stack([d, -e]), q - p)
    return p + t[0] * d


def flats_stage(only=None):
    """Each plate that is a rectangle in a photo: its four sides from picked edge chains, each a line fitted along the whole side, the corners where the lines meet, the photo flattened through them."""
    import evidence as E
    os.makedirs(FLATS, exist_ok=True)
    for d in C.load(C.CONTROL)['faces']:
        if 'from_photo' not in d or (only and d['id'] not in only):
            continue
        f = Face(d)
        t = d['tile']
        ch = E.tile_chains(d['from_photo'], t['box'], t.get('sigma', 2.5), t.get('min', 70))
        lines, rms, n = {}, {}, {}
        for side in ('top', 'right', 'bottom', 'left'):
            P = np.vstack([side_points(ch, pick, side) for pick in d['edges'][side]])
            lines[side] = fit_line(P)
            c, v = lines[side]
            rms[side] = round(float(np.sqrt(np.mean(((P - c) @ np.array([-v[1], v[0]])) ** 2))), 2)
            n[side] = int(len(P))
        quad = np.array([meet(lines['top'], lines['left']), meet(lines['top'], lines['right']), meet(lines['bottom'], lines['right']), meet(lines['bottom'], lines['left'])])
        H = cv2.getPerspectiveTransform(quad.astype(np.float32), f.corners_flat().astype(np.float32))
        img = C.photo(d['from_photo'])
        flat = cv2.warpPerspective(img, H, f.flat_size, flags=cv2.INTER_CUBIC)
        cv2.imwrite(f.flat, flat)
        for side, (c, v) in lines.items():
            cv2.line(img, tuple(int(q) for q in c - 3000 * v), tuple(int(q) for q in c + 3000 * v), (0, 0, 255), 1, cv2.LINE_AA)
        cv2.polylines(img, [quad.astype(np.int32)], True, (0, 255, 255), 2, cv2.LINE_AA)
        cv2.imwrite(os.path.join(FLATS, d['id'] + '_quad.jpg'), img, [cv2.IMWRITE_JPEG_QUALITY, 90])
        C.save(os.path.join(FLATS, d['id'] + '.json'), {'quad_px': quad.round(2).tolist(), 'side_rms_px': rms, 'side_points': n, 'H': H.tolist()})
        print('%-10s from %s: corners %s, sides straight to %s px over %s points, flat %dx%d' % (d['id'], d['from_photo'], quad.round(1).tolist(), rms, n, flat.shape[1], flat.shape[0]))


# ------------------------------------------------------------------------------------------------ matching
_M = {}


def match(a_bgr, b_bgr):
    from lightglue import ALIKED, LightGlue
    from lightglue.utils import numpy_image_to_torch, rbd
    import torch
    if not _M:
        _M['e'] = ALIKED(max_num_keypoints=4096, detection_threshold=0.005).eval().cuda()
        _M['m'] = LightGlue(features='aliked', filter_threshold=0.1).eval().cuda()
    with torch.inference_mode():
        fa = _M['e'].extract(numpy_image_to_torch(cv2.cvtColor(a_bgr, cv2.COLOR_BGR2RGB)).cuda(), resize=None)
        fb = _M['e'].extract(numpy_image_to_torch(cv2.cvtColor(b_bgr, cv2.COLOR_BGR2RGB)).cuda(), resize=None)
        r = rbd(_M['m']({'image0': fa, 'image1': fb}))
        fa, fb = rbd(fa), rbd(fb)
    k = r['matches'].cpu().numpy()
    return fa['keypoints'].cpu().numpy()[k[:, 0]], fb['keypoints'].cpu().numpy()[k[:, 1]]


def prewarp(face, cam, flat, w, h):
    """The flat as the camera would see the face, or None when it cannot."""
    centre = face.body(face.corners_flat()).mean(0)
    if np.dot(face.n, cam.centre() - centre) <= 0.05:
        return None
    x0, y0, x1, y1 = face.rect
    gx, gy = np.meshgrid(np.linspace(x0, x1, 9), np.linspace(y0, y1, 9))
    fl = (np.stack([gx.ravel(), gy.ravel()], 1) + face.margin) * face.ppm
    px = C.project(cam, face.body(fl))
    ok = np.isfinite(px).all(1)
    inside = ok & (px[:, 0] > 0) & (px[:, 0] < w) & (px[:, 1] > 0) & (px[:, 1] < h)
    if ok.sum() < 20 or inside.sum() < 6 or np.ptp(px[inside], axis=0).max() < 120:
        return None
    H, _ = cv2.findHomography(fl[ok], px[ok], 0)
    warped = cv2.warpPerspective(flat, H, (w, h), flags=cv2.INTER_AREA)
    m = cv2.warpPerspective(np.full(flat.shape[:2], 255, np.uint8), H, (w, h)) > 0
    ys, xs = np.nonzero(m)
    if len(xs) < 5000:
        return None
    pad = 60
    return H, warped, (max(xs.min() - pad, 0), min(xs.max() + pad, w), max(ys.min() - pad, 0), min(ys.max() + pad, h))


def find_face(face, flat, img, cam):
    """Matches between the flat (its pixels) and the photo that lie on the face: the best of three direct tries and one through the camera."""
    h, w = img.shape[:2]
    tries = []
    for width in (1800, 1100, 650):
        s = width / flat.shape[1]
        a, b = match(cv2.resize(flat, None, fx=s, fy=s, interpolation=cv2.INTER_AREA), img)
        tries.append(('direct %d' % width, a / s, b))
    pw = prewarp(face, cam, flat, w, h) if cam is not None else None
    if pw is not None:
        H, warped, (x0, x1, y0, y1) = pw
        a, b = match(warped[y0:y1, x0:x1], img[y0:y1, x0:x1])
        if len(a):
            a = cv2.perspectiveTransform((a + [x0, y0]).reshape(-1, 1, 2).astype(np.float64), np.linalg.inv(H)).reshape(-1, 2)
            tries.append(('through the camera', a, b + [x0, y0]))
    best = (0, None, None, '')
    for how, fl, px in tries:
        if len(fl) < 12:
            continue
        Hm, inl = cv2.findHomography(fl, px, cv2.RANSAC, 3.0)
        if inl is None:
            continue
        inl = inl.ravel().astype(bool)
        if inl.sum() > best[0]:
            best = (int(inl.sum()), fl[inl], px[inl].astype(np.float64), how)
    return best


def match_stage(only=None):
    fs = faces()
    rec, names = load_solve()
    old = dict(np.load(MATCHES, allow_pickle=True)['sets'].item()) if os.path.exists(MATCHES) else {}
    flats = {k: cv2.imread(f.flat) for k, f in fs.items()}
    for name in sorted(names):
        img = None
        for fid, face in fs.items():
            key = '%s %s' % (name, fid)
            if only and fid not in only:
                continue
            views = face.d.get('views')
            if views and name not in views:
                continue
            if img is None:
                img = C.photo(name)
            n, fl, px, how = find_face(face, flats[fid], img, camera_of(rec, names[name]) if name not in skip() else None)
            if n >= MIN_ON_FACE:
                old[key] = {'view': name, 'face': fid, 'px': px, 'flat': fl, 'how': how}
                print('%-7s %-8s %5d on the face (%s)' % (name, fid, n, how), flush=True)
            else:
                old.pop(key, None)
    os.makedirs(C.OUT, exist_ok=True)
    np.savez_compressed(MATCHES, sets=np.array(old, dtype=object))
    return old


def control_sets():
    return [s for s in dict(np.load(MATCHES, allow_pickle=True)['sets'].item()).values() if len(s['px']) >= MIN_FOR_CONTROL]


# ------------------------------------------------------------------------------------------------ the adjustment
def load_solve():
    import pycolmap
    a = C.load(ALIGN)
    rec = pycolmap.Reconstruction(MODEL)
    rec.transform(pycolmap.Sim3d(a['scale'], pycolmap.Rotation3d(np.array(a['R'])), np.array(a['t'])))
    return rec, {im.name[:-4]: i for i, im in rec.images.items()}


def camera_of(rec, image_id):
    im = rec.images[image_id]
    c = rec.cameras[im.camera_id]
    M = im.cam_from_world().matrix() if callable(im.cam_from_world) else im.cam_from_world.matrix()
    f, cx, cy, k = c.params
    return C.Camera(f, cx, cy, M[:3, :3], M[:3, 3], c.width, c.height, k)


def adjust(sets, fs, use, pp_free=(), intrinsics=None, seed=0):
    """One bundle adjustment from the solve as it was, with the control of the sets for which use(set) holds, at the faces' present places.
    pp_free: the views whose principal point may move; every other view then keeps the intrinsics given for it."""
    import pycolmap
    rec, names = load_solve()
    sk = skip()
    cfg = pycolmap.BundleAdjustmentConfig()
    for name, i in names.items():
        if name not in sk:
            cfg.add_image(i)
    tie = list(rec.point3D_ids())
    rng = np.random.default_rng(seed)
    for s in sets:
        if s['view'] in sk or not use(s):
            continue
        im = rec.images[names[s['view']]]
        X = fs[s['face']].body(s['flat'])
        for j in rng.choice(len(X), min(PER_FACE, len(X)), replace=False):
            idx = im.num_points2D()
            im.points2D.append(pycolmap.Point2D(s['px'][j].astype(np.float64)))
            im.points2D.append(pycolmap.Point2D(s['px'][j].astype(np.float64)))      # COLMAP's adjuster wants two sightings of a point: the same one twice
            cfg.add_constant_point(rec.add_point3D(X[j].astype(np.float64), pycolmap.Track([pycolmap.TrackElement(im.image_id, idx), pycolmap.TrackElement(im.image_id, idx + 1)])))
    for pid in tie:
        cfg.add_variable_point(pid)
    o = pycolmap.BundleAdjustmentOptions()
    o.refine_focal_length, o.refine_extra_params, o.print_summary = True, True, False
    o.refine_principal_point = bool(pp_free)
    if pp_free:
        for name, i in names.items():
            if name in sk or name in pp_free:
                continue
            cam = rec.cameras[rec.images[i].camera_id]
            if intrinsics and name in intrinsics:
                cam.params = np.array(intrinsics[name], float)
            cfg.set_constant_cam_intrinsics(cam.camera_id)
    o.ceres.loss_function_type = pycolmap.LossFunctionType.CAUCHY
    o.ceres.loss_function_scale = 2.0
    o.ceres.solver_options.max_num_iterations = 200
    pycolmap.create_default_bundle_adjuster(o, cfg, rec).solve()
    return rec, names


def fit_plate(face, sets, cams):
    """A plate's place, turn, scale (and aspect) through the given cameras."""
    from scipy.optimize import least_squares
    ss = [s for s in sets if s['face'] == face.id and s['view'] in cams]
    if not ss:
        return None
    ab = [face.ab(s['flat']) for s in ss]
    free = np.array([1, 1, 1, 1, 1, 1, 'scale' in face.free, 'aspect' in face.free], float)
    q0 = face.q.copy()

    def fun(x):
        q = q0 + x * free
        return np.concatenate([(C.project(cams[s['view']], face.place(a, b, q)) - s['px']).ravel() for s, (a, b) in zip(ss, ab)])
    r = least_squares(fun, np.zeros(8), loss='huber', f_scale=3.0, x_scale=np.array([0.05] * 3 + [0.05] * 3 + [0.05, 0.05]))
    face.q = q0 + r.x * free
    return float(np.linalg.norm(r.x[3:6]))


def residuals(sets, fs, cams):
    out = {}
    for s in sets:
        if s['view'] not in cams:
            continue
        d = np.linalg.norm(C.project(cams[s['view']], fs[s['face']].body(s['flat'])) - s['px'], axis=1)
        d = d[np.isfinite(d)]
        if len(d):
            out.setdefault(s['view'], {})[s['face']] = {'matches': int(len(d)), 'median_px': round(float(np.median(d)), 2), 'p90_px': round(float(np.percentile(d, 90)), 2),
                                                        'bbox_px': [round(float(v)) for v in (*s['px'].min(0), *s['px'].max(0))]}
    return out


def tie_stats(rec, names):
    out = {}
    for name, i in names.items():
        if name in skip():
            continue
        im, cam, e = rec.images[i], camera_of(rec, i), []
        for p in im.points2D:
            if p.has_point3D():
                pt = rec.point3D(p.point3D_id)
                if len({el.image_id for el in pt.track.elements}) >= 2:
                    e.append(np.linalg.norm(C.project(cam, pt.xyz)[0] - p.xy))
        out[name] = {'ties': len(e), 'tie_px': round(float(np.median(e)), 2) if e else None}
    return out


def cams_of(rec, names):
    return {name: camera_of(rec, i) for name, i in names.items() if name not in skip()}


def solve_stage(leave_out=True):
    sets = control_sets()
    fs = faces()
    plates = [f for f in fs.values() if f.kind == 'plate']
    frame = [f.id for f in fs.values() if f.kind == 'frame']
    has_frame = {s['view'] for s in sets if s['face'] in frame}
    # 1. the plates' places: first through cameras that the frame's face alone holds, then round and round with the plates in
    rec, names = adjust(sets, fs, lambda s: s['face'] in frame)
    cams = cams_of(rec, names)
    for p in plates:
        if not getattr(p, 'placed', True):
            own = [s for s in sets if s['face'] == p.id and s['view'] == p.source]
            if not own:
                raise SystemExit('plate %s has no matches in its own photo %s: run match first' % (p.id, p.source))
            p.start_from(cams[p.source], own[0]['flat'], own[0]['px'])
            print('plate %s starts at %s facing %s' % (p.id, p.c.round(3).tolist(), p.n.round(2).tolist()))
    for r in range(ROUNDS + 2):
        cams = cams_of(rec, names)
        moved = [fit_plate(p, sets, cams) for p in plates]
        print('round %d: plates moved %s m' % (r, ['%.3f' % m for m in moved if m is not None]), flush=True)
        rec, names = adjust(sets, fs, lambda s: True)
        if r >= 1 and all(m is None or m < 0.002 for m in moved):
            break
    held = {name: rec.cameras[rec.images[i].camera_id].params.tolist() for name, i in names.items()}
    # 2. the principal point: free in the views that see control (the prints are cropped), believed only within a bound
    with_control = {s['view'] for s in sets if s['view'] not in skip()}
    pp_free = set(with_control)
    for r in range(ROUNDS + 2):
        rec, names = adjust(sets, fs, lambda s: True, pp_free=pp_free, intrinsics=held)
        cams = cams_of(rec, names)
        off = {n for n in pp_free if np.hypot(cams[n].cx - cams[n].w / 2, cams[n].cy - cams[n].h / 2) > PP_BOUND * max(cams[n].w, cams[n].h)}
        moved = [fit_plate(p, sets, cams) for p in plates]
        print('principal point round %d: plates moved %s m; not believed in %s' % (r, ['%.3f' % m for m in moved if m is not None], sorted(off) or 'none'), flush=True)
        if off:
            pp_free -= off
            continue
        if all(m is None or m < 0.002 for m in moved):
            break
    rec, names = adjust(sets, fs, lambda s: True, pp_free=pp_free, intrinsics=held)
    cams = cams_of(rec, names)
    res, ties = residuals(sets, fs, cams), tie_stats(rec, names)
    views = {}
    for name in sorted(cams):
        ctrl = res.get(name, {})
        worst = max((v['median_px'] for v in ctrl.values()), default=None)
        t = ties[name]
        grade = 'A' if ctrl and worst <= A_LIMIT_PX and (t['tie_px'] is None or t['tie_px'] <= TIE_LIMIT_PX) else 'B'
        views[name] = {'grade': grade, 'camera': cams[name].to_dict(), 'control': ctrl, 'ties': t['ties'], 'tie_px': t['tie_px'], 'principal_point_free': name in pp_free, 'eye_m': cams[name].centre().round(3).tolist()}
    for name in skip():
        views[name] = {'grade': 'C', 'why': 'square-on to one plane, or not solved: a source of flats only'}
    # 3. each view with control once without it: how far its control then misses is what a view like it can be trusted to, away from control
    if leave_out:
        for name in sorted(with_control):
            r2, n2 = adjust(sets, fs, lambda s, v=name: s['view'] != v, pp_free=pp_free - {name}, intrinsics=held)
            c2 = {name: camera_of(r2, n2[name])}
            miss = residuals([s for s in sets if s['view'] == name], fs, c2).get(name, {})
            views[name]['without_own_control_px'] = round(max(v['median_px'] for v in miss.values()), 1) if miss else None
            print('%-7s without its own control: %s px' % (name, views[name]['without_own_control_px']), flush=True)
    doc = {'how': 'tools/recon/osa/cameras.py: the joint solve adjusted with the control faces of control.json (docs/OSA_MODELLING_PROCESS.md stage 1)', 'frame': frame, 'limits': {'A_control_px': A_LIMIT_PX, 'tie_px': TIE_LIMIT_PX, 'principal_point_bound': PP_BOUND},
           'faces': {f.id: f.summary() for f in fs.values()}, 'views': views}
    C.save(C.CAMERAS, doc)
    for name, v in views.items():
        if v['grade'] != 'C':
            print('%-7s %s  control %s  ties %4d at %s px  without its own control %s' % (name, v['grade'], {k: q['median_px'] for k, q in v['control'].items()} or '-', v['ties'], v['tie_px'], v.get('without_own_control_px', '-')))
    for f in plates:
        print('plate %-8s %s' % (f.id, {k: v for k, v in f.summary().items() if k in ('moved_m', 'turned_deg', 'scale', 'aspect')}))
    return doc


def show_stage(only=None):
    doc = C.load(C.CAMERAS)
    fs = faces()
    for f in fs.values():
        f.restore(doc['faces'][f.id])
    sets = control_sets()
    d = os.path.join(C.OUT, 'cameras')
    os.makedirs(d, exist_ok=True)
    for name, v in doc['views'].items():
        if v['grade'] == 'C' or (only and name not in only):
            continue
        img, cam = C.photo(name), C.Camera.from_dict(v['camera'])
        h, w = img.shape[:2]
        for f in fs.values():
            p = C.project(cam, f.outline_body())
            z = cam.cam(f.outline_body())[:, 2]
            ok = (z > 0.25) & np.isfinite(p).all(1) & (p[:, 0] > -w) & (p[:, 0] < 2 * w) & (p[:, 1] > -h) & (p[:, 1] < 2 * h)
            for i in range(len(p) - 1):
                if ok[i] and ok[i + 1]:
                    cv2.line(img, tuple(int(q) for q in p[i]), tuple(int(q) for q in p[i + 1]), (255, 255, 0) if f.kind == 'frame' else (0, 220, 0), 2, cv2.LINE_AA)
        for s in sets:
            if s['view'] == name:
                pr = C.project(cam, fs[s['face']].body(s['flat']))
                for a, b in list(zip(pr, s['px']))[::max(1, len(pr) // 150)]:
                    cv2.line(img, tuple(int(q) for q in a), tuple(int(q) for q in b), (0, 0, 255), 2, cv2.LINE_AA)
        cv2.putText(img, '%s grade %s  control %s' % (name, v['grade'], {k: q['median_px'] for k, q in v['control'].items()}), (10, 24), 0, 0.6, (0, 0, 0), 4, cv2.LINE_AA)
        cv2.putText(img, '%s grade %s  control %s' % (name, v['grade'], {k: q['median_px'] for k, q in v['control'].items()}), (10, 24), 0, 0.6, (255, 255, 255), 1, cv2.LINE_AA)
        cv2.imwrite(os.path.join(d, name + '.jpg'), img, [cv2.IMWRITE_JPEG_QUALITY, 90])
    print('drawn in', d)


if __name__ == '__main__':
    stage = sys.argv[1]
    if stage == 'flats':
        flats_stage(sys.argv[2:] or None)
    elif stage == 'match':
        match_stage(sys.argv[2:] or None)
    elif stage == 'solve':
        solve_stage('--quick' not in sys.argv)
    elif stage == 'show':
        show_stage(sys.argv[2:] or None)


# ------------------------------------------------------------------------------------------------ tie points labelled by an outline
def points_in(view, outline_px, grades=('A', 'B')):
    """The joint solve's tie points that this photo sees inside an outline, each placed again by the certified cameras of
    the photos that see it (rays meeting, least squares). A point is labelled by the outline it falls in, not by judgement.
    Returns [(pixel in this view, body point, photos used, worst reprojection px)]."""
    import pycolmap
    rec = pycolmap.Reconstruction(MODEL)
    names = {im.name[:-4]: i for i, im in rec.images.items()}
    by_id = {i: n for n, i in names.items()}
    cams = {k: v['camera'] for k, v in C.cameras().items() if v['grade'] in grades}
    poly = np.asarray(outline_px, np.float32)
    out = []
    im = rec.images[names[view]]
    for p in im.points2D:
        if not p.has_point3D() or cv2.pointPolygonTest(poly, (float(p.xy[0]), float(p.xy[1])), False) < 0:
            continue
        obs = []
        for el in rec.point3D(p.point3D_id).track.elements:
            n = by_id[el.image_id]
            if n in cams:
                obs.append((n, np.array(rec.images[el.image_id].points2D[el.point2D_idx].xy, float)))
        if len(obs) < 2:
            continue
        A_, b_ = np.zeros((3, 3)), np.zeros(3)
        for n, xy in obs:
            d = C.rays(cams[n], [xy])[0]
            M = np.eye(3) - np.outer(d, d)
            A_ += M
            b_ += M @ cams[n].centre()
        X = np.linalg.solve(A_, b_)
        err = max(float(np.linalg.norm(C.project(cams[n], X)[0] - xy)) for n, xy in obs)
        out.append((np.array(p.xy, float), X, [n for n, _ in obs], err))
    return out
