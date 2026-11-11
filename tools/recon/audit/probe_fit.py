"""Probe for the new process's fit stage (docs/OSA_MODELLING_PROCESS.md, stage 5): one simple part, the first-aid box,
as a part object with physical parameters (a rounded box: a place, a turn, three sizes, a corner radius), fitted to
its outlines in two photographs at once through the cameras of probe_cameras.py. Nothing is written to the model.

    python tools/recon/audit/probe_fit.py

Out: out/recon/audit/fit/first_aid_<photo>.jpg (photo outline green, the model's box as it is red, the fitted box
yellow), fit.json.
"""
import json
import os
import sys

import cv2
import numpy as np
from scipy.optimize import least_squares

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as A  # noqa: E402
import part_audit as PA  # noqa: E402
from common import DF, V  # noqa: E402

OUT = os.path.join(A.OUT, 'fit')
VIEWS = ('g098-a', 'g096-a')
PART = 'first_aid'
_DIRS = np.array([[x, y, z] for x in (-1, 0, 1) for y in (-1, 0, 1) for z in (-1, 0, 1) if (x, y, z) != (0, 0, 0)], float)
_DIRS /= np.linalg.norm(_DIRS, axis=1, keepdims=True)


def rounded_box(q):
    """Points whose hull is the box: q = centre (3), rotation vector (3), log half sizes (3), log corner radius."""
    c, R, half, r = q[:3], cv2.Rodrigues(q[3:6])[0], np.exp(q[6:9]), np.exp(q[9])
    r = min(r, 0.95 * half.min())
    corners = np.array([[sx, sy, sz] for sx in (-1, 1) for sy in (-1, 1) for sz in (-1, 1)], float) * (half - r)
    pts = (corners[:, None, :] + r * _DIRS[None, :, :]).reshape(-1, 3)
    return c + (R @ pts.T).T


def outline(cam, pts):
    p = A.project(cam, pts)
    return cv2.convexHull(p.astype(np.float32)).reshape(-1, 2)


def dense(poly, step=3.0):
    out = []
    for a, b in zip(poly, np.roll(poly, -1, axis=0)):
        n = max(1, int(np.linalg.norm(b - a) / step))
        out.append(a + (b - a) * np.linspace(0, 1, n, endpoint=False)[:, None])
    return np.vstack(out)


class View:
    def __init__(self, name, cam):
        self.name, self.cam = name, cam
        m = cv2.imread(os.path.join(PA.MASKS, name, PART + '.png'), 0) > 0
        c = PA.contour(m)
        self.contour = c[::max(1, len(c) // 300)].astype(np.float32)
        e = np.ones(m.shape, np.uint8)
        e[c[:, 1], c[:, 0]] = 0
        self.dt = cv2.distanceTransform(e, cv2.DIST_L2, 5)
        self.mask = m

    def residual(self, pts):
        """Both ways: the model's outline to the photo's, and the photo's outline to the model's."""
        hull = outline(self.cam, pts)
        d = dense(hull)
        a = cv2.remap(self.dt, d[:, 0].astype(np.float32), d[:, 1].astype(np.float32), cv2.INTER_LINEAR, borderMode=cv2.BORDER_REPLICATE).ravel()
        a = np.interp(np.linspace(0, 1, 200), np.linspace(0, 1, len(a)), a)
        b = np.array([abs(cv2.pointPolygonTest(hull, (float(x), float(y)), True)) for x, y in self.contour])
        return a, b


def start_from_model(scene):
    """The model's own first-aid box as the start: its triangles' centre, axes and extents."""
    P = scene.tri[scene.part == scene.names.index(PART)].reshape(-1, 3)
    c = 0.5 * (P.min(0) + P.max(0))
    u, s, vt = np.linalg.svd(P - P.mean(0), full_matrices=False)
    R = vt.T
    if np.linalg.det(R) < 0:
        R[:, 2] *= -1
    L = (P - P.mean(0)) @ R
    half = 0.5 * (L.max(0) - L.min(0))
    c = P.mean(0) + R @ (0.5 * (L.max(0) + L.min(0)))
    return np.concatenate([c, cv2.Rodrigues(R)[0].ravel(), np.log(half), [np.log(0.02)]])


def main():
    os.makedirs(OUT, exist_ok=True)
    cams = json.load(open(os.path.join(A.OUT, 'probe_cameras_solved_pp.json')))['cameras']
    views = [View(n, V.Camera.from_dict(cams[n])) for n in VIEWS]
    scene = DF.Scene()
    q0 = start_from_model(scene)

    def fun(q):
        return np.concatenate([np.concatenate(v.residual(rounded_box(q))) for v in views])

    def report(q):
        return {v.name: {'photo_to_model_median_px': round(float(np.median(v.residual(rounded_box(q))[1])), 1), 'photo_to_model_p90_px': round(float(np.percentile(v.residual(rounded_box(q))[1], 90)), 1),
                         'share_of_width': round(float(np.median(v.residual(rounded_box(q))[1])) / v.cam.w, 4)} for v in views}
    best = None
    for jitter in range(6):                                  # a few starts: a box seen from two sides has more than one way to sit
        rng = np.random.default_rng(jitter)
        q = q0.copy()
        if jitter:
            q[:3] += rng.normal(0, 0.03, 3)
            q[3:6] += rng.normal(0, 0.15, 3)
            q[6:9] += rng.normal(0, 0.15, 3)
        r = least_squares(fun, q, loss='soft_l1', f_scale=4.0, x_scale=np.array([0.05] * 3 + [0.2] * 3 + [0.2] * 3 + [0.5]), diff_step=1e-3, max_nfev=400)
        if best is None or r.cost < best.cost:
            best = r
    q1 = best.x
    out = {'views': list(VIEWS), 'cameras': 'probe_cameras_solved_pp.json', 'as the model has it': report(q0), 'fitted': report(q1),
           'model_size_m': [round(float(2 * v), 3) for v in sorted(np.exp(q0[6:9]), reverse=True)], 'fitted_size_m': [round(float(2 * v), 3) for v in sorted(np.exp(q1[6:9]), reverse=True)],
           'fitted_corner_radius_m': round(float(np.exp(q1[9])), 3), 'moved_m': [round(float(v), 3) for v in q1[:3] - q0[:3]]}
    J = best.jac
    try:
        cov = np.linalg.inv(J.T @ J) * (2 * best.cost / max(1, len(best.fun) - len(q1)))
        out['sigma_centre_m'] = [round(float(v), 4) for v in np.sqrt(np.diag(cov))[:3]]
        out['sigma_size_share'] = [round(float(v), 3) for v in np.sqrt(np.diag(cov))[6:9]]
    except np.linalg.LinAlgError:
        pass
    for v in views:
        img = A.photo(v.name)
        cv2.drawContours(img, cv2.findContours(v.mask.astype(np.uint8), cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)[0], -1, (0, 255, 0), 2)
        cv2.polylines(img, [outline(v.cam, rounded_box(q0)).astype(np.int32)], True, (0, 0, 255), 2, cv2.LINE_AA)
        cv2.polylines(img, [outline(v.cam, rounded_box(q1)).astype(np.int32)], True, (0, 230, 255), 2, cv2.LINE_AA)
        ys, xs = np.nonzero(v.mask)
        pad = 220
        x0, x1, y0, y1 = max(0, xs.min() - pad), min(img.shape[1], xs.max() + pad), max(0, ys.min() - pad), min(img.shape[0], ys.max() + pad)
        cv2.imwrite(os.path.join(OUT, '%s_%s.jpg' % (PART, v.name)), img[y0:y1, x0:x1], [cv2.IMWRITE_JPEG_QUALITY, 92])
    with open(os.path.join(OUT, 'fit.json'), 'w', encoding='utf-8') as f:
        json.dump(out, f, indent=1)
    for k, val in out.items():
        print(k, val)


if __name__ == '__main__':
    main()
