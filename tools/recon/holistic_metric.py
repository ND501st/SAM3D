"""Fuse a photo's depth map with what is known in metres (docs/OSA_INTERIOR_HOLISTIC.md section 34).

A depth network is right about shape and wrong about distance. The joint solve and the millimetre traces are the other way round: exact, but only at a few places. This puts them together:
the map is multiplied by exp(d), where d is a field across the image that is smooth except across the map's own depth edges, and is pinned to

- the **perspective**: the solved camera and every triangulated point this photo saw (its true range along its ray), and
- the **traces**: the parts traced in millimetres, cast through the same camera (their true range at every pixel they cover).

So the shape between the pins stays the network's and the distances become the pins'. A pin that disagrees with its neighbours (a cable in front of a rack) is turned down robustly.

**Withheld pins are the test.** A quarter of the image's cells keep their pins back and the field is solved without them; the miss on those pins, before and after, is what `holdout_*_m` reports.
Pinned pixels agree with the model by construction, so they are no evidence about it: the comparison scores the rest, and uses a view only if its withheld pins are met.

Numpy only (a matrix-free conjugate gradient, coarse to fine), so the tests need nothing installed.
"""
import numpy as np

LONG = 400              # the field's working size
SIGMA_POINT = 0.02      # a pin's error, as a share of the range
SIGMA_TRACE = 0.02
TRACE_STRIDE = 4        # one trace pin every this many working pixels
SMOOTH = 300.0          # how hard the field resists changing from pixel to pixel (log units)
EDGE = 0.03             # the map's own depth steps loosen the field: a step in log depth this large cuts its stiffness to 1/e
EDGE_FLOOR = 0.003
PRIOR = 0.03            # the pull back to no correction, where nothing pins the field
HUBER = 0.04            # a pin further from the field than this share is turned down
ROUNDS = 4
GROSS = 0.3             # a pin this far (log) from the view's median correction is not on what it claims to be
HOLD_CELLS = 8          # the image is cut in this many cells a side, for holding pins back
HOLD_SHARE = 0.25
MIN_PINS = 40


def _edges(logz):
    def weight(step):
        return np.clip(np.nan_to_num(np.exp(-np.abs(step) / EDGE), nan=EDGE_FLOOR), EDGE_FLOOR, 1.0)
    return weight(np.diff(logz, axis=1)), weight(np.diff(logz, axis=0))


def _apply(x, D, gx, gy, lam):
    out = D * x
    fx = lam * gx * (x[:, 1:] - x[:, :-1])
    out[:, :-1] -= fx
    out[:, 1:] += fx
    fy = lam * gy * (x[1:] - x[:-1])
    out[:-1] -= fy
    out[1:] += fy
    return out


def _diagonal(D, gx, gy, lam):
    m = D.copy()
    m[:, :-1] += lam * gx
    m[:, 1:] += lam * gx
    m[:-1] += lam * gy
    m[1:] += lam * gy
    return m


def _cg(x, b, D, gx, gy, lam, iters=240, tol=1e-5):
    M = _diagonal(D, gx, gy, lam)
    r = b - _apply(x, D, gx, gy, lam)
    z = r / M
    p = z.copy()
    rz = float(np.sum(r * z))
    stop = tol * tol * max(float(np.sum(b * b / M)), 1e-30)
    for _ in range(iters):
        if rz <= stop:
            break
        Ap = _apply(p, D, gx, gy, lam)
        a = rz / float(np.sum(p * Ap))
        x = x + a * p
        r = r - a * Ap
        z = r / M
        rz2 = float(np.sum(r * z))
        p = z + (rz2 / rz) * p
        rz = rz2
    return x


def _down(a, how):
    h, w = a.shape[0] // 2, a.shape[1] // 2
    b = a[:2 * h, :2 * w].reshape(h, 2, w, 2)
    if how == 'sum':
        return b.sum(axis=(1, 3))
    ok = np.isfinite(b)
    n = ok.sum(axis=(1, 3))
    s = np.where(ok, b, 0.0).sum(axis=(1, 3))
    return np.where(n > 0, s / np.maximum(n, 1), np.nan)


def _up(a, shape):
    b = np.repeat(np.repeat(a, 2, axis=0), 2, axis=1)
    return np.pad(b, ((0, max(0, shape[0] - b.shape[0])), (0, max(0, shape[1] - b.shape[1]))), mode='edge')[:shape[0], :shape[1]]


def solve_field(logz, u, v, target, weight, smooth=SMOOTH, prior=PRIOR, init=None):
    """The field d on logz's grid minimising sum weight (d(u, v) - target)^2 + smooth |grad d|^2 (loosened across logz's steps) + prior d^2."""
    h, w = logz.shape
    D, b = np.zeros((h, w)), np.zeros((h, w))
    i, j = np.clip(np.rint(v).astype(int), 0, h - 1), np.clip(np.rint(u).astype(int), 0, w - 1)
    np.add.at(D, (i, j), weight)
    np.add.at(b, (i, j), weight * target)
    if init is not None:
        gx, gy = _edges(logz)
        return _cg(init, b, D + prior, gx, gy, smooth)
    levels = [(logz, D, b)]
    while min(levels[-1][0].shape) >= 24:
        z_, D_, b_ = levels[-1]
        levels.append((_down(z_, 'mean'), _down(D_, 'sum'), _down(b_, 'sum')))
    x = np.zeros_like(levels[-1][0])
    for n, (z_, D_, b_) in enumerate(reversed(levels)):
        if n:
            x = _up(x, z_.shape)
        gx, gy = _edges(z_)
        x = _cg(x, b_, D_ + prior, gx, gy, smooth)
    return x


def _resize_nearest(a, h, w):
    H, W = a.shape[:2]
    return a[((np.arange(h) + 0.5) * H / h).astype(int)][:, ((np.arange(w) + 0.5) * W / w).astype(int)]


def _resize_linear(a, h, w):
    H, W = a.shape
    y = np.clip((np.arange(h) + 0.5) * H / h - 0.5, 0, H - 1)
    x = np.clip((np.arange(w) + 0.5) * W / w - 0.5, 0, W - 1)
    y0, x0 = np.minimum(y.astype(int), H - 2), np.minimum(x.astype(int), W - 2)
    fy, fx = (y - y0)[:, None], (x - x0)[None, :]
    return (a[y0][:, x0] * (1 - fx) + a[y0][:, x0 + 1] * fx) * (1 - fy) + (a[y0 + 1][:, x0] * (1 - fx) + a[y0 + 1][:, x0 + 1] * fx) * fy


def point_pins(z, cam, X):
    """Triangulated points seen in this photo: pixel x, y and the true z along the camera's axis."""
    if X is None or not len(X):
        return np.zeros(0), np.zeros(0), np.zeros(0)
    Xc = cam.cam(X)
    px = cam.project(X)
    h, w = z.shape
    ok = np.isfinite(px).all(1) & (px[:, 0] >= 0) & (px[:, 0] < w - 1) & (px[:, 1] >= 0) & (px[:, 1] < h - 1) & (Xc[:, 2] > 0.05)
    return px[ok, 0], px[ok, 1], Xc[ok, 2]


def trace_pins(z, cam, scene, anchored, rays_of, keep=None, stride=8):
    """The traced parts cast through the camera: pixel x, y and the true z, where the ray's first hit is a traced part."""
    h, w = z.shape
    y, x = np.mgrid[stride // 2:h:stride, stride // 2:w:stride]
    x, y = x.ravel().astype(float), y.ravel().astype(float)
    c = rays_of(cam, x + 0.5, y + 0.5)
    nrm = np.linalg.norm(c, axis=1)
    tri, t = scene.cast(cam.centre(), (cam.R.T @ (c / nrm[:, None]).T).T)
    hit = tri >= 0
    hit[hit] = anchored[scene.part[tri[hit]]]
    if keep is not None:
        hit &= keep[y.astype(int), x.astype(int)]
    return x[hit], y[hit], (t / nrm)[hit]


def _fit(logz, u, v, lr, w0, smooth, prior):
    d, w = None, w0
    for _ in range(ROUNDS):
        d = solve_field(logz, u, v, lr, w, smooth, prior, init=d)
        h, ww = logz.shape
        r = lr - d[np.clip(np.rint(v).astype(int), 0, h - 1), np.clip(np.rint(u).astype(int), 0, ww - 1)]
        w = w0 / np.maximum(1.0, np.abs(r) / HUBER)
    return d


def fuse_map(z, pins_point, pins_trace, seed=0, smooth=SMOOTH, prior=PRIOR):
    """z with its pins applied. Pins are (x, y, z_true) at z's size. Returns the fused z and a report; with too few pins, z unchanged."""
    H, W = z.shape
    s = LONG / max(H, W)
    h, w = max(8, round(H * s)), max(8, round(W * s))
    zs = np.where(np.isfinite(z) & (z > 0.02), z, np.nan)
    logz = np.log(_resize_nearest(zs, h, w))
    x = np.concatenate([pins_point[0], pins_trace[0]])
    y = np.concatenate([pins_point[1], pins_trace[1]])
    zt = np.concatenate([pins_point[2], pins_trace[2]])
    kind = np.concatenate([np.zeros(len(pins_point[0]), int), np.ones(len(pins_trace[0]), int)])
    zm = zs[np.minimum(y.astype(int), H - 1), np.minimum(x.astype(int), W - 1)]
    ok = np.isfinite(zm) & (zt > 0.05)
    report = {'pins_points': int((ok & (kind == 0)).sum()), 'pins_traces': int((ok & (kind == 1)).sum()), 'holdout_before_m': None, 'holdout_after_m': None, 'held_pins': 0}
    if ok.sum() < MIN_PINS:
        report['skipped'] = 'too few pins'
        return z, report
    x, y, zt, zm, kind = x[ok], y[ok], zt[ok], zm[ok], kind[ok]
    lr = np.log(zt / zm)
    near = np.abs(lr - np.median(lr)) < GROSS
    x, y, zt, zm, kind, lr = x[near], y[near], zt[near], zm[near], kind[near], lr[near]
    u, v = x * s - 0.5, y * s - 0.5
    sig = np.where(kind == 0, SIGMA_POINT, SIGMA_TRACE)
    w0 = 1.0 / sig ** 2 / np.where(kind == 1, TRACE_STRIDE, 1)
    rng = np.random.default_rng(seed)
    cells = rng.random((HOLD_CELLS, HOLD_CELLS)) < HOLD_SHARE
    held = cells[np.minimum((y * HOLD_CELLS / H).astype(int), HOLD_CELLS - 1), np.minimum((x * HOLD_CELLS / W).astype(int), HOLD_CELLS - 1)]
    if held.sum() >= 8 and (~held).sum() >= MIN_PINS:
        d = _fit(logz, u[~held], v[~held], lr[~held], w0[~held], smooth, prior)
        dh = d[np.clip(np.rint(v[held]).astype(int), 0, h - 1), np.clip(np.rint(u[held]).astype(int), 0, w - 1)]
        report['holdout_before_m'] = round(float(np.median(np.abs(zm[held] - zt[held]))), 4)
        report['holdout_after_m'] = round(float(np.median(np.abs(zm[held] * np.exp(dh) - zt[held]))), 4)
        report['held_pins'] = int(held.sum())
    d = _fit(logz, u, v, lr, w0, smooth, prior)
    return z * np.exp(_resize_linear(d, H, W)), report
