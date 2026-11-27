"""Cameras, the reference-mesh prior and model-based fitting (docs/OSA_EXTERIOR_RECONSTRUCTION.md, stages S4-S7, S10).

Book photos are cropped reproductions of photos of different vehicles on different days: two-view geometry between them
is weak, so cameras are found against a 3-D prior instead (analysis by synthesis). The prior is the reference mesh,
cut into parts by its group names, scaled to sourced dimensions and articulated (turret yaw, launcher and antenna
pitch). A photo's camera is a pinhole with a free principal point (cropping moves it) and one radial term.

* `load_obj`: vertices, triangles and group names of an OBJ, without a third-party loader.
* `Prior`: the mesh in the vehicle frame (x right, y forward, z up, metres, origin on the ground under the middle
  of the wheelbase), its parts, `scaled_to` sourced dimensions, `posed` for an articulation.
* `Camera`: intrinsics and pose, `project`, `look_at` from a reader's azimuth and height.
* `silhouette`: the posed prior's mask in a camera (triangles filled by OpenCV).
* `fit_camera`: the camera (and the photo's articulation) that best explains the photo's vehicle mask and edges,
  started from the catalogue's view; tries the mirrored photo too (books flip photos).
* `pnp`: the camera from named keypoints (hubs, corners) when a person or a detector has marked them.
* `grabcut_mask`: a vehicle mask without SAM 2: GrabCut from the photo's frame.
* `overlay_score`: the acceptance numbers for a photo (silhouette IoU, edge chamfer in px).

Frames: the project's vehicle frame matches data/vehicles/*.json (x right, y forward, z up). OpenCV's camera frame is
x right, y down, z forward.
"""
import math


def _np():
    import numpy as np
    return np


def _cv():
    import cv2
    return cv2


# ---------------------------------------------------------------------------------------------------------- the mesh

def load_obj(path):
    """(V [n,3], F [m,3] zero-based triangles, groups {name: triangle indices})."""
    np = _np()
    V, F, G = [], [], {}
    cur = 'default'
    with open(path, encoding='utf-8', errors='replace') as fh:
        for line in fh:
            if line.startswith('v '):
                V.append([float(t) for t in line.split()[1:4]])
            elif line.startswith('f '):
                idx = [int(t.split('/')[0]) for t in line.split()[1:]]
                idx = [i - 1 if i > 0 else len(V) + i for i in idx]
                for k in range(1, len(idx) - 1):  # fan-triangulate
                    G.setdefault(cur, []).append(len(F))
                    F.append([idx[0], idx[k], idx[k + 1]])
            elif line.startswith(('g ', 'o ')):
                cur = line.strip()[2:]
    return np.array(V, float), np.array(F, np.int64), {k: np.array(v, np.int64) for k, v in G.items()}


def part_of(group, rules):
    """The part a group belongs to by the config's substring rules, e.g. {"turret": ["turret", "antenna"]}."""
    g = group.lower()
    for part, keys in rules.items():
        if any(k in g for k in keys):
            return part
    return 'hull'


class Prior:
    """The reference mesh in the vehicle frame, cut into parts."""

    def __init__(self, V, F, groups, part_rules, axes=('-x', 'z', 'y'), front_sign=1.0):
        np = _np()
        # axes: which signed source axis becomes the vehicle's x (right), y (forward), z (up). A Y-up OBJ facing +Z
        # is ('-x', 'z', 'y'): facing +Z with +Y up, the right hand points along -X. front_sign = -1 turns the
        # model half round (x and y both flipped, still right-handed) when its bow is at the negative end.
        k = {'x': 0, 'y': 1, 'z': 2}

        def axis(a):
            return (-1.0 if a.startswith('-') else 1.0) * V[:, k[a.lstrip('-')]]
        W = np.stack([axis(axes[0]) * front_sign, axis(axes[1]) * front_sign, axis(axes[2])], axis=1)
        if np.linalg.det(np.array([[1.0 if a.lstrip('-') == c else 0.0 for c in 'xyz'] for a in axes])
                         * np.array([[-1.0 if a.startswith('-') else 1.0] for a in axes])) < 0:
            raise ValueError('axes %s make a left-handed (mirrored) frame' % (axes,))
        W[:, 2] -= W[:, 2].min()  # wheels on the ground
        self.V, self.F = W, F
        self.part = np.array(['hull'] * len(F), dtype=object)
        for g, tris in groups.items():
            self.part[tris] = part_of(g, part_rules)
        self.pivots = {}

    def size(self):
        return self.V.max(axis=0) - self.V.min(axis=0)

    def scaled_to(self, length_m=None, width_m=None, height_m=None):
        """Scale each axis to a sourced overall dimension (only the ones given); returns the scale factors."""
        np = _np()
        s = np.ones(3)
        sz = self.size()
        for i, v in enumerate((width_m, length_m, height_m)):
            if v:
                s[i] = v / sz[i]
        c = (self.V.max(axis=0) + self.V.min(axis=0)) / 2
        c[2] = 0.0
        self.V = (self.V - c) * s
        return s

    def sampled(self, n=400000, seed=0):
        """Surface samples for silhouettes (self.S) with their parts (self.S_part)."""
        self.S, tri = surface_samples(self.V, self.F, n, seed)
        self.S_part = self.part[tri]
        return self

    def posed(self, art, samples=True):
        """The samples (or the vertices) with the articulation applied: art = {"turret_yaw_deg": ..}. The turret,
        launcher and antenna parts turn about the pivot in self.pivots['turret'] (vehicle frame)."""
        np = _np()
        X = (self.S if samples else self.V).copy()
        yaw = math.radians((art or {}).get('turret_yaw_deg', 0.0))
        if yaw and 'turret' in self.pivots:
            moving = ['turret', 'launcher', 'antenna']
            if samples:
                idx = np.isin(self.S_part, moving)
            else:
                idx = np.unique(self.F[np.isin(self.part, moving)])
            p = np.array(self.pivots['turret'], float)
            c, s = math.cos(yaw), math.sin(yaw)
            R = np.array([[c, -s, 0], [s, c, 0], [0, 0, 1]])
            X[idx] = (X[idx] - p) @ R.T + p
        return X


# ------------------------------------------------------------------------------------------------------- the camera

class Camera:
    """Pinhole: f (px), principal point (cx, cy), radial k1; pose as R (world->camera) and t."""

    def __init__(self, f, cx, cy, R, t, k1=0.0, w=None, h=None):
        self.f, self.cx, self.cy, self.R, self.t, self.k1, self.w, self.h = f, cx, cy, R, t, k1, w, h

    def K(self):
        np = _np()
        return np.array([[self.f, 0, self.cx], [0, self.f, self.cy], [0, 0, 1.0]])

    def project(self, X):
        np = _np()
        Xc = X @ self.R.T + self.t
        z = np.maximum(Xc[:, 2], 1e-6)
        x, y = Xc[:, 0] / z, Xc[:, 1] / z
        r2 = x * x + y * y
        d = 1 + self.k1 * r2
        return np.stack([self.f * x * d + self.cx, self.f * y * d + self.cy], axis=1), Xc[:, 2]

    def to_dict(self):
        return {'f': self.f, 'cx': self.cx, 'cy': self.cy, 'k1': self.k1, 'R': self.R.tolist(), 't': self.t.tolist(),
                'w': self.w, 'h': self.h}

    @staticmethod
    def from_dict(d):
        np = _np()
        return Camera(d['f'], d['cx'], d['cy'], np.array(d['R']), np.array(d['t']), d.get('k1', 0.0), d.get('w'), d.get('h'))


def look_at(eye, target, up=(0, 0, 1.0)):
    """R, t of a camera at eye looking at target (vehicle frame, z up) in OpenCV's camera frame."""
    np = _np()
    eye, target, up = np.array(eye, float), np.array(target, float), np.array(up, float)
    z = target - eye
    z /= np.linalg.norm(z)
    x = np.cross(z, up)
    if np.linalg.norm(x) < 1e-9:
        x = np.array([1.0, 0, 0])
    x /= np.linalg.norm(x)
    y = np.cross(z, x)
    R = np.stack([x, y, z])
    return R, -R @ eye


def camera_from_view(az_deg, height, dist_m, w, h, fov_deg=40.0, target=(0, 0, 1.4)):
    """A starting camera from the catalogue's view: azimuth round the vehicle (0 = ahead of the bow, 90 = right side,
    clockwise from above), a height word or metres, a distance."""
    heights = {'ground': 0.4, 'eye': 1.6, 'raised': 3.0, 'roof': 4.0, 'top-down': 12.0, 'underneath': 0.2}
    hz = heights.get(str(height).split()[0].lower(), 1.6) if not isinstance(height, (int, float)) else float(height)
    a = math.radians(az_deg)
    eye = (dist_m * math.sin(a), dist_m * math.cos(a), hz)
    R, t = look_at(eye, target)
    f = 0.5 * w / math.tan(math.radians(fov_deg) / 2)
    return Camera(f, w / 2, h / 2, R, t, 0.0, w, h)


def rodrigues(r):
    cv2, np = _cv(), _np()
    return cv2.Rodrigues(np.asarray(r, float).reshape(3, 1))[0]


def pnp(points3d, points2d, w, h, f_range=(0.5, 6.0), steps=24):
    """Camera from named correspondences with an unknown focal length: a grid over f (in units of the photo's width),
    solvePnP at each, keep the lowest reprojection error, then refine f, the principal point and k1 together.
    Needs at least 6 points spread in depth; returns (Camera, rms_px)."""
    np, cv2 = _np(), _cv()
    X = np.asarray(points3d, float)
    x = np.asarray(points2d, float)
    best = None
    for f in np.geomspace(f_range[0] * w, f_range[1] * w, steps):
        K = np.array([[f, 0, w / 2], [0, f, h / 2], [0, 0, 1.0]])
        ok, rv, tv = cv2.solvePnP(X, x, K, None, flags=cv2.SOLVEPNP_ITERATIVE)
        if not ok:
            continue
        proj, _ = cv2.projectPoints(X, rv, tv, K, None)
        err = float(np.sqrt(np.mean(np.sum((proj.reshape(-1, 2) - x) ** 2, axis=1))))
        if best is None or err < best[0]:
            best = (err, f, rv.ravel(), tv.ravel())
    if best is None:
        return None, float('inf')
    _, f, rv, tv = best

    def res(p):
        cam = Camera(p[0], p[1], p[2], rodrigues(p[4:7]), p[7:10], p[3], w, h)
        u, _ = cam.project(X)
        return (u - x).ravel()
    p0 = np.concatenate([[f, w / 2, h / 2, 0.0], rv, tv])
    p = levenberg_marquardt(res, p0, prior=lambda p: np.array([(p[1] - w / 2) / (0.25 * w), (p[2] - h / 2) / (0.25 * h), p[3] / 0.2]))
    cam = Camera(p[0], p[1], p[2], rodrigues(p[4:7]), p[7:10], p[3], w, h)
    u, _ = cam.project(X)
    return cam, float(np.sqrt(np.mean(np.sum((u - x) ** 2, axis=1))))


# ------------------------------------------------------------------------------------------------------- optimisers

def levenberg_marquardt(res, p0, prior=None, iters=60, lam=1e-3):
    """Small dense LM with a numerical Jacobian (parameter counts here are tens, not thousands)."""
    np = _np()
    p = np.array(p0, float)

    def full(q):
        r = res(q)
        return np.concatenate([r, prior(q)]) if prior else r
    r = full(p)
    cost = r @ r
    for _ in range(iters):
        J = np.empty((len(r), len(p)))
        for j in range(len(p)):
            h = 1e-6 * max(1.0, abs(p[j]))
            q = p.copy()
            q[j] += h
            J[:, j] = (full(q) - r) / h
        A = J.T @ J
        g = J.T @ r
        while True:
            try:
                step = np.linalg.solve(A + lam * np.diag(np.diag(A) + 1e-12), -g)
            except np.linalg.LinAlgError:
                lam *= 10
                continue
            q = p + step
            rq = full(q)
            cq = rq @ rq
            if cq < cost:
                p, r, cost, lam = q, rq, cq, max(lam / 3, 1e-9)
                break
            lam *= 4
            if lam > 1e9:
                return p
        if np.linalg.norm(step) < 1e-10 * (1 + np.linalg.norm(p)):
            break
    return p


def nelder_mead(fun, x0, scale, iters=300, tol=1e-6):
    """Derivative-free minimisation for the silhouette objective (it is piecewise constant at the pixel level)."""
    np = _np()
    n = len(x0)
    pts = [np.array(x0, float)] + [np.array(x0, float) + np.eye(n)[i] * scale[i] for i in range(n)]
    vals = [fun(p) for p in pts]
    for _ in range(iters):
        order = np.argsort(vals)
        pts = [pts[i] for i in order]
        vals = [vals[i] for i in order]
        if abs(vals[-1] - vals[0]) < tol:
            break
        c = np.mean(pts[:-1], axis=0)
        xr = c + (c - pts[-1])
        fr = fun(xr)
        if fr < vals[0]:
            xe = c + 2 * (c - pts[-1])
            fe = fun(xe)
            pts[-1], vals[-1] = (xe, fe) if fe < fr else (xr, fr)
        elif fr < vals[-2]:
            pts[-1], vals[-1] = xr, fr
        else:
            xc = c + 0.5 * (pts[-1] - c)
            fc = fun(xc)
            if fc < vals[-1]:
                pts[-1], vals[-1] = xc, fc
            else:
                pts = [pts[0]] + [pts[0] + 0.5 * (p - pts[0]) for p in pts[1:]]
                vals = [vals[0]] + [fun(p) for p in pts[1:]]
    i = int(np.argmin(vals))
    return pts[i], vals[i]


# ----------------------------------------------------------------------------------------------- silhouettes, fitting

def surface_samples(V, F, n=400000, seed=0):
    """n points spread over the triangles by area, with each point's triangle (for its part label)."""
    np = _np()
    a, b, c = V[F[:, 0]], V[F[:, 1]], V[F[:, 2]]
    area = 0.5 * np.linalg.norm(np.cross(b - a, c - a), axis=1)
    rng = np.random.default_rng(seed)
    tri = rng.choice(len(F), size=n, p=area / area.sum())
    u, v = rng.random(n), rng.random(n)
    flip = u + v > 1
    u[flip], v[flip] = 1 - u[flip], 1 - v[flip]
    return a[tri] + u[:, None] * (b[tri] - a[tri]) + v[:, None] * (c[tri] - a[tri]), tri


def silhouette(V, F, cam, w, h, close_px=3):
    """uint8 mask of the mesh in the camera (255 inside): the triangles filled one by one (exact, slow; for checks
    and small meshes). Use `splat` inside optimisers."""
    np, cv2 = _np(), _cv()
    u, z = cam.project(V)
    ok = (z[F] > 0.05).all(axis=1)
    tri = np.round(u[F[ok]] * 4).astype(np.int32)  # 2 bits of sub-pixel precision
    m = np.zeros((h, w), np.uint8)
    for t in tri:
        cv2.fillConvexPoly(m, t, 255, lineType=cv2.LINE_8, shift=2)
    return m


def splat(P, cam, w, h, close_px=3):
    """uint8 mask from the surface samples P (n, 3): each sample marks its pixel, then a closing fills the gaps.
    A few milliseconds for 400 000 samples: the silhouette objective's renderer."""
    np, cv2 = _np(), _cv()
    u, z = cam.project(P)
    ui = np.round(u).astype(np.int64)
    ok = (z > 0.05) & (ui[:, 0] >= 0) & (ui[:, 0] < w) & (ui[:, 1] >= 0) & (ui[:, 1] < h)
    m = np.zeros((h, w), np.uint8)
    m[ui[ok, 1], ui[ok, 0]] = 255
    if close_px > 1:
        m = cv2.morphologyEx(m, cv2.MORPH_CLOSE, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (close_px, close_px)))
    return m


def edges_distance(gray):
    """Distance (px) to the nearest edge of the descreened photo."""
    cv2, np = _cv(), _np()
    e = cv2.Canny(cv2.GaussianBlur(gray, (0, 0), 1.0), 40, 120)
    return cv2.distanceTransform(255 - e, cv2.DIST_L2, 3).astype(np.float32)


def overlay_score(model_mask, photo_mask, edge_dt=None):
    """(silhouette IoU, mean distance in px from the model's outline to the photo's edges)."""
    np, cv2 = _np(), _cv()
    a, b = model_mask > 0, photo_mask > 0
    inter, union = np.logical_and(a, b).sum(), np.logical_or(a, b).sum()
    iou = float(inter / union) if union else 0.0
    ch = None
    if edge_dt is not None:
        cnts, _ = cv2.findContours(model_mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)
        pts = np.concatenate([c.reshape(-1, 2) for c in cnts]) if cnts else np.zeros((0, 2), int)
        if len(pts):
            ch = float(edge_dt[pts[:, 1].clip(0, edge_dt.shape[0] - 1), pts[:, 0].clip(0, edge_dt.shape[1] - 1)].mean())
    return iou, ch


def fit_camera(prior, photo_mask, start, edge_dt=None, art=None, free_art=(), iters=250, chamfer_weight=0.01):
    """Refine a camera (and named articulation angles) so the prior's silhouette matches the photo's vehicle mask.
    Parameters: rotation vector, translation, log focal, principal point shift, articulation. Returns
    (Camera, articulation, cost)."""
    np = _np()
    h, w = photo_mask.shape
    rv0 = _cv().Rodrigues(start.R)[0].ravel()
    art = dict(art or {})
    x0 = np.concatenate([rv0, start.t, [math.log(start.f), 0.0, 0.0], [art.get(k, 0.0) for k in free_art]])
    scale = np.concatenate([[0.05] * 3, [0.3, 0.3, 0.6], [0.1, 0.02 * w, 0.02 * h], [10.0] * len(free_art)])

    def unpack(x):
        cam = Camera(math.exp(x[6]), w / 2 + x[7], h / 2 + x[8], rodrigues(x[0:3]), x[3:6], 0.0, w, h)
        a = dict(art)
        a.update({k: x[9 + i] for i, k in enumerate(free_art)})
        return cam, a

    def cost(x):
        cam, a = unpack(x)
        m = splat(prior.posed(a), cam, w, h)
        iou, ch = overlay_score(m, photo_mask, edge_dt)
        return (1.0 - iou) + (chamfer_weight * ch if ch is not None else 0.0)
    x, c = nelder_mead(cost, x0, scale, iters=iters)
    cam, a = unpack(x)
    return cam, a, c


def fit_camera_or_mirror(prior, photo_mask, start, edge_dt=None, **kw):
    """Books print some photos mirrored: fit the photo and its mirror image, keep the better, say which."""
    np = _np()
    cam, a, c = fit_camera(prior, photo_mask, start, edge_dt, **kw)
    m2 = np.ascontiguousarray(photo_mask[:, ::-1])
    e2 = np.ascontiguousarray(edge_dt[:, ::-1]) if edge_dt is not None else None
    cam2, a2, c2 = fit_camera(prior, m2, start, e2, **kw)
    if c2 < 0.8 * c:
        return cam2, a2, c2, True
    return cam, a, c, False


def grabcut_mask(bgr, margin=0.03, iters=5):
    """A vehicle mask without SAM 2: GrabCut started from the photo minus a thin border."""
    cv2, np = _cv(), _np()
    h, w = bgr.shape[:2]
    rect = (int(margin * w), int(margin * h), int((1 - 2 * margin) * w), int((1 - 2 * margin) * h))
    mask = np.zeros((h, w), np.uint8)
    bgd, fgd = np.zeros((1, 65), np.float64), np.zeros((1, 65), np.float64)
    cv2.grabCut(bgr, mask, rect, bgd, fgd, iters, cv2.GC_INIT_WITH_RECT)
    return np.where((mask == cv2.GC_FGD) | (mask == cv2.GC_PR_FGD), 255, 0).astype(np.uint8)
