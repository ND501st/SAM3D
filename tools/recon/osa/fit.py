"""Stage 5 of docs/OSA_MODELLING_PROCESS.md: shape fitted to outlines in all views at once.

The primitives every part's fit is made of. All residuals are in pixels of the photographs:

    plane_of_curves   one flat outline seen in two or more photos: the plane on which the photos' outlines, cast back
                      through their cameras, lie on one another
    curve_on_plane    the photos' outlines on that plane as one line, in the plane's own coordinates
    depth_of          how far a profile is carried back, from the outline of its far end
    outline_error     a 3-D line through a camera against the photo's outline: the acceptance measure

Cameras are certified ones (core.cameras()) and are never moved here.
"""
import os
import sys

import numpy as np
from scipy.optimize import least_squares

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import core as C  # noqa: E402
from parts import Frame, unit  # noqa: E402

FAR_PX = 40.0             # an outline point further than this from the other photo's outline is not the same stretch (hidden there, or not picked)


from core import outline_error, pts, seg_dist  # noqa: E402,F401  (the acceptance measure lives in core: the tests use it without SciPy)


def cast(cam, px, n, d):
    """Pixels cast through a camera onto the plane n.X = d."""
    r = C.rays(cam, pts(px))
    eye = cam.centre()
    t = (d - n @ eye) / (r @ n)
    X = eye + r * t[:, None]
    X[t <= 0] = np.nan
    return X


def _plane(q, n0, u, v, d0):
    n = unit(n0 + q[0] * u + q[1] * v)
    return n, d0 + q[2]


def plane_of_curves(obs, n0, d0, free_turn=True, groups=None):
    """obs: [(camera, outline px N x 2)], the same flat outline in each. n0, d0: where to start (a normal and n.X).
    groups: one label per observation when several outlines share the plane (two parts' faces in one wall): only
    observations with the same label are laid on one another. Returns n, d, and what is left per pair of views (px)."""
    n0 = unit(n0)
    u = unit(np.cross(n0, [0, 0, 1.0])) if abs(n0[2]) < 0.9 else np.array([1.0, 0, 0])
    v = np.cross(n0, u)

    def pairs(q):
        n, d = _plane(q * [free_turn, free_turn, 1], n0, u, v, d0)
        out = []
        for i, (ci, pi) in enumerate(obs):
            X = cast(ci, pi, n, d)
            for j, (cj, pj) in enumerate(obs):
                if i == j or (groups is not None and groups[i] != groups[j]):
                    continue
                p = C.project(cj, X[np.isfinite(X).all(1)])
                e = seg_dist(p[np.isfinite(p).all(1)], pj)
                out.append((i, j, e))
        return out

    def fun(q):
        return np.concatenate([np.minimum(e, FAR_PX) for _, _, e in pairs(q)])
    best = None
    for dd in (0.0, -0.15, 0.15, -0.3, 0.3):
        r = least_squares(fun, [0, 0, dd], loss='soft_l1', f_scale=3.0, x_scale=[0.1, 0.1, 0.05], diff_step=1e-4)
        if best is None or r.cost < best.cost:
            best = r
    n, d = _plane(best.x * [free_turn, free_turn, 1], n0, u, v, d0)
    left = {}
    for i, j, e in pairs(best.x):
        k = e < FAR_PX
        left['%d->%d' % (i, j)] = {'median_px': round(float(np.median(e[k])), 2) if k.any() else None, 'p90_px': round(float(np.percentile(e[k], 90)), 2) if k.any() else None, 'share_seen': round(float(k.mean()), 2)}
    return n, d, left


def curve_on_plane(obs, frame, lead=0, step_m=0.004, merge_m=0.012):
    """The outlines on the frame's plane as one line (a, b metres): the lead view's line, each point moved to the mean of what the views put there."""
    n, d = frame.normal, float(frame.normal @ frame.origin)
    lines = []
    for cam, px in obs:
        X = cast(cam, px, n, d)                    # for one line out of several pieces, hand in evidence's `line` (ordered), not the pieces
        lines.append(frame.coords(X[np.isfinite(X).all(1)])[0])
    ref = lines[lead]
    s = np.concatenate([[0], np.cumsum(np.linalg.norm(np.diff(ref, axis=0), axis=1))])
    t = np.arange(0, s[-1], step_m)
    ref = np.stack([np.interp(t, s, ref[:, 0]), np.interp(t, s, ref[:, 1])], 1)
    acc, cnt = ref.copy(), np.ones(len(ref))
    for k, L in enumerate(lines):
        if k == lead or len(L) < 2:
            continue
        a, b = L[:-1], L[1:]
        ab = b - a
        L2 = np.maximum(np.sum(ab * ab, axis=1), 1e-12)
        tt = np.clip(np.einsum('nsk,sk->ns', ref[:, None, :] - a[None], ab) / L2, 0, 1)
        foot = a[None] + tt[..., None] * ab[None]
        dist = np.linalg.norm(ref[:, None, :] - foot, axis=2)
        j = dist.argmin(axis=1)
        near = dist[np.arange(len(ref)), j] < merge_m
        acc[near] += foot[np.arange(len(ref)), j][near]
        cnt[near] += 1
    out = acc / cnt[:, None]
    for k_, L in enumerate(lines):                 # what another view's line has beyond either end of the lead's is added on
        if k_ == lead or len(L) < 3:
            continue
        s_ = np.concatenate([[0], np.cumsum(np.linalg.norm(np.diff(L, axis=0), axis=1))])
        tt = np.arange(0, s_[-1], step_m)
        L = np.stack([np.interp(tt, s_, L[:, 0]), np.interp(tt, s_, L[:, 1])], 1)
        far = seg_dist(L, out) > merge_m
        for run in (L[:int(np.argmin(far))] if far[0] else None, L[len(L) - int(np.argmin(far[::-1])):] if far[-1] else None):
            if run is None or len(run) * step_m < 2 * merge_m:
                continue
            inner = run[-1] if run is not None and far[0] and run is not None and np.array_equal(run[0], L[0]) else run[0]
            if np.linalg.norm(inner - out[0]) < np.linalg.norm(inner - out[-1]):
                out = np.vstack([run if np.array_equal(inner, run[-1]) else run[::-1], out])
            else:
                out = np.vstack([out, run if np.array_equal(inner, run[0]) else run[::-1]])
    k = 5
    return np.stack([np.convolve(np.pad(out[:, i], k // 2, mode='edge'), np.ones(k) / k, mode='valid') for i in (0, 1)], 1)


def depth_of(obs, frame, profile, d0=0.2, lo=0.01, hi=1.5):
    """How far the profile is carried back along the normal: the back's outline through each camera against the photos' outline of the far end."""
    def fun(q):
        back = frame.at(profile, -q[0])
        out = []
        for cam, px in obs:
            p = C.project(cam, back)
            p = p[np.isfinite(p).all(1)]
            out.append(np.minimum(seg_dist(pts(px), p), FAR_PX))
        return np.concatenate(out)
    best = None
    for start in (d0, 0.5 * d0, 2 * d0, 0.1, 0.4):
        r = least_squares(fun, [start], bounds=([lo], [hi]), loss='soft_l1', f_scale=3.0, diff_step=1e-3)
        if best is None or r.cost < best.cost:
            best = r
    e = fun(best.x)
    return float(best.x[0]), {'median_px': round(float(np.median(e)), 2), 'p90_px': round(float(np.percentile(e, 90)), 2)}


def frame_for(n, d, near, up_hint=(0, 0, 1.0)):
    return Frame.on_plane(n, d, near, up_hint)


def star_profile(obs, frame, bin_deg=2.5, min_pts=2):
    """Several photos' outlines of one flat, rounded outline, cast onto the frame's plane and made one line: every point is
    taken by its bearing from the outline's middle, and each bearing keeps the median distance of what the photos put
    there. Bearings no photo covers stay empty (the hidden side). Returns (a, b) points in order of bearing, and the middle."""
    n, d = frame.normal, float(frame.normal @ frame.origin)
    P = []
    for cam, px in obs:
        X = cast(cam, px, n, d)
        P.append(frame.coords(X[np.isfinite(X).all(1)])[0])
    P = np.vstack(P)
    c = 0.5 * (P.min(0) + P.max(0))
    th = np.degrees(np.arctan2(P[:, 1] - c[1], P[:, 0] - c[0]))
    r = np.linalg.norm(P - c, axis=1)
    out = []
    for t0 in np.arange(-180, 180, bin_deg):
        k = (th >= t0) & (th < t0 + bin_deg)
        if k.sum() >= min_pts:
            t = np.radians(np.median(th[k]))
            out.append((t, float(np.median(r[k]))))
    out = np.array(out)
    # start after the widest gap, so the line runs round the seen part in one piece
    gaps = np.diff(np.concatenate([out[:, 0], [out[0, 0] + 2 * np.pi]]))
    i = int(np.argmax(gaps)) + 1
    out = np.roll(out, -i, axis=0)
    line = c + out[:, 1:2] * np.stack([np.cos(out[:, 0]), np.sin(out[:, 0])], 1)
    k = 3
    line = np.stack([np.convolve(np.pad(line[:, j], k // 2, mode='edge'), np.ones(k) / k, mode='valid') for j in (0, 1)], 1)
    return line, c, float(np.degrees(gaps.max()))


def rounded_box(obs, start, starts=6, seed=0):
    """A box with rounded edges fitted to its outline in several photos at once. obs: [(camera, closed outline px)];
    start: a parts.RoundedBox. Returns the fitted RoundedBox and what is left per view (px, the photo's outline to the box's)."""
    import cv2
    from parts import RoundedBox

    def unpack(q):
        return RoundedBox(q[:3], q[3:6], 2 * np.exp(q[6:9]), float(np.exp(q[9])), start.material)

    def hull(cam, box):
        p = C.project(cam, box.hull_points())
        return cv2.convexHull(p[np.isfinite(p).all(1)].astype(np.float32)).reshape(-1, 2)

    def fun(q):
        box, out = unpack(q), []
        for cam, line in obs:
            h = hull(cam, box)
            hd = np.vstack([h, h[:1]])
            out.append(seg_dist(pts(line), hd))
            dense = np.vstack([a + (b - a) * np.linspace(0, 1, 12, endpoint=False)[:, None] for a, b in zip(hd[:-1], hd[1:])])
            e = seg_dist(dense, np.vstack([pts(line), pts(line)[:1]]))
            out.append(np.interp(np.linspace(0, 1, 150), np.linspace(0, 1, len(e)), e))
        return np.concatenate(out)
    q0 = np.concatenate([start.centre, start.rotvec, np.log(0.5 * start.size), [np.log(max(start.radius, 0.005))]])
    best = None
    rng = np.random.default_rng(seed)
    for k in range(starts):
        q = q0.copy()
        if k:
            q[:3] += rng.normal(0, 0.03, 3)
            q[3:6] += rng.normal(0, 0.15, 3)
            q[6:9] += rng.normal(0, 0.15, 3)
        r = least_squares(fun, q, loss='soft_l1', f_scale=4.0, x_scale=np.array([0.05] * 3 + [0.2] * 3 + [0.2] * 3 + [0.5]), diff_step=1e-3, max_nfev=400)
        if best is None or r.cost < best.cost:
            best = r
    box = unpack(best.x)
    left = []
    for cam, line in obs:
        h = hull(cam, box)
        e = seg_dist(pts(line), np.vstack([h, h[:1]]))
        left.append({'median_px': round(float(np.median(e)), 2), 'p90_px': round(float(np.percentile(e, 90)), 2)})
    return box, left
