"""Shared pieces of the Osa modelling process (docs/OSA_MODELLING_PROCESS.md): files, photos, cameras, projection.

Everything derived from the book's photographs is written under out/recon/osa/ (git-ignored). What is kept in the
repository are numbers only: picks, measured outlines, cameras, part parameters (tools/traces/osa_interior/, data/).

Run with E:/SAM3D-tools/venv-depth/Scripts/python.exe -X utf8.
"""
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
RECON = os.path.dirname(HERE)
REPO = os.path.dirname(os.path.dirname(RECON))
for p in (RECON, os.path.join(REPO, 'tools', 'blender')):
    if p not in sys.path:
        sys.path.insert(0, p)
import holistic_view as V  # noqa: E402  (the camera class, the cabin's panels, the photos' files)

Camera = V.Camera
OUT = os.path.join(REPO, 'out', 'recon', 'osa')
TRACES = os.path.join(REPO, 'tools', 'traces', 'osa_interior')
CONTROL = os.path.join(TRACES, 'control.json')
CAMERAS = os.path.join(TRACES, 'cameras.json')
EVIDENCE = os.path.join(TRACES, 'evidence')
PARTS = os.path.join(REPO, 'data', 'cabins', 'osa_ak_parts.json')


def load(path):
    with open(path, encoding='utf-8') as f:
        return json.load(f)


def save(path, doc, indent=1):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        json.dump(doc, f, indent=indent, ensure_ascii=False)
        f.write('\n')


def photo(name, kind='clean'):
    import cv2
    p = V.photo_file(name, kind)
    if p is None:
        raise FileNotFoundError('no %s photo %s: the book run lives in out/recon (docs/OSA_MODELLING_HANDOFF.md section 0)' % (kind, name))
    return cv2.imread(p)


def project(cam, X):
    """Pixels of body points on the photo as scanned: the pinhole and its radial term. NaN behind the camera."""
    import cv2
    X = np.atleast_2d(np.asarray(X, float))
    Xc = cam.cam(X)
    K = np.array([[cam.f, 0, cam.cx], [0, cam.f, cam.cy], [0, 0, 1.0]])
    p, _ = cv2.projectPoints(Xc.reshape(-1, 1, 3), np.zeros(3), np.zeros(3), K, np.array([cam.k1, 0, 0, 0.0]))
    p = p.reshape(-1, 2)
    p[Xc[:, 2] <= 1e-3] = np.nan
    return p


def rays(cam, px):
    """Unit rays in the body frame through pixels of the photo as scanned (the lens term taken out exactly)."""
    import cv2
    px = np.atleast_2d(np.asarray(px, float))
    K = np.array([[cam.f, 0, cam.cx], [0, cam.f, cam.cy], [0, 0, 1.0]])
    n = cv2.undistortPoints(px.reshape(-1, 1, 2), K, np.array([cam.k1, 0, 0, 0.0])).reshape(-1, 2)
    d = np.column_stack([n, np.ones(len(n))])
    d = (cam.R.T @ d.T).T
    return d / np.linalg.norm(d, axis=1, keepdims=True)


_CAMS = {}


def cameras():
    """The certified cameras (cameras.py solve): {photo: {'camera': Camera, 'grade': 'A'|'B'|'C', ...}}."""
    if not _CAMS:
        doc = load(CAMERAS)
        for k, v in doc['views'].items():
            _CAMS[k] = dict(v, camera=Camera.from_dict(v['camera']) if 'camera' in v else None)
    return _CAMS


def camera(name, grade=None):
    c = cameras()[name]
    if grade and c['grade'] > grade:
        raise ValueError('%s is grade %s: it may not be used where grade %s is asked for' % (name, c['grade'], grade))
    return c['camera']


# ------------------------------------------------------------------------------------------------ the acceptance measure
def seg_dist(P, Q):
    """Distance from each point of P to the polyline Q, or to the nearest of several polylines (px)."""
    if isinstance(Q, (list, tuple)):
        return np.min([seg_dist(P, q) for q in Q if len(q) >= 2], axis=0)
    P, Q = np.asarray(P, float), np.asarray(Q, float)
    a, b = Q[:-1], Q[1:]
    ab = b - a
    L2 = np.maximum(np.sum(ab * ab, axis=1), 1e-12)
    out = np.full(len(P), np.inf)
    for i in range(0, len(P), 256):
        p = P[i:i + 256]
        t = np.clip(np.einsum('nsk,sk->ns', p[:, None, :] - a[None], ab) / L2, 0, 1)
        d = np.linalg.norm(p[:, None, :] - (a[None] + t[..., None] * ab[None]), axis=2)
        out[i:i + 256] = d.min(axis=1)
    return out


def pts(px):
    """An outline's points, whether it is one line or several pieces."""
    return np.vstack(px) if isinstance(px, (list, tuple)) else np.asarray(px, float)


def outline_error(cam, X, px, closed=False, both=False, hull=False):
    """A photo's outline against a 3-D line through its camera: distance from each photo point to the projected line (px).
    hull: X is a cloud of points whose silhouette is the outline (a box with rounded edges)."""
    p = project(cam, X)
    p = p[np.isfinite(p).all(1)]
    if hull and len(p) >= 3:
        import cv2
        p = cv2.convexHull(p.astype(np.float32)).reshape(-1, 2).astype(float)
        closed = True
    if closed and len(p):
        p = np.vstack([p, p[:1]])
    if len(p) < 2:
        return np.full(len(pts(px)), np.nan)
    e = seg_dist(pts(px), p)
    if both:
        return e, seg_dist(p, px)
    return e
