"""Shared pieces of the accuracy audit (docs/OSA_MODELLING_HANDOFF.md section 8): cameras, photos, the two faces whose
millimetre flats exist, projection with the lens term. Nothing here changes the model or the pipeline; everything is
written under out/recon/audit/.

Run the audit scripts with E:/SAM3D-tools/venv-depth/Scripts/python.exe -X utf8.
"""
import os
import sys

import cv2
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
RECON = os.path.dirname(HERE)
sys.path.insert(0, RECON)
sys.path.insert(0, os.path.join(os.path.dirname(RECON), 'blender'))
import holistic_diff as DF  # noqa: E402
import holistic_view as V  # noqa: E402

REPO = V.REPO
OUT = os.path.join(REPO, 'out', 'recon', 'audit')
REF = os.path.join(DF.OUT, 'ref')


def photo(name, kind='clean'):
    return cv2.imread(V.photo_file(name, kind))


def registered_views():
    return sorted(f[:-4] for f in os.listdir(REF) if f.endswith('.npz'))


def reg_camera(name, w=None, h=None):
    """The camera every judgement of the last session was made through (holistic_diff's ref), at a given size."""
    _, cam = DF.registered_camera(name)
    return cam.scaled(w, h) if w else cam


def solve_camera(name, w, h):
    c = V.Camera.from_dict(V.load(V.SFM_CAMERAS)['cameras'][name])
    return c.scaled(w, h)


def project(cam, X):
    """Pixels of body points on the photo as scanned: the pinhole and its radial term. NaN behind the camera."""
    X = np.atleast_2d(np.asarray(X, float))
    Xc = cam.cam(X)
    K = np.array([[cam.f, 0, cam.cx], [0, cam.f, cam.cy], [0, 0, 1.0]])
    p, _ = cv2.projectPoints(Xc.reshape(-1, 1, 3), np.zeros(3), np.zeros(3), K, np.array([cam.k1, 0, 0, 0.0]))
    p = p.reshape(-1, 2)
    p[Xc[:, 2] <= 1e-3] = np.nan
    return p


class Face:
    """A face traced in millimetres with its flattened photo: flat pixels to the body frame."""

    def __init__(self, name, image, ppm, margin, panel, to_ab, rect_mm, source):
        self.name, self.image, self.ppm, self.margin, self.panel, self._ab, self.rect_mm, self.source = name, image, ppm, margin, panel, to_ab, rect_mm, source
        self.n, self.r, self.up = V.panel_axes(panel)
        self.centre = np.array(panel['centre_m'], float)

    def mm(self, uv):
        uv = np.atleast_2d(np.asarray(uv, float))
        return uv / self.ppm - self.margin

    def body(self, uv):
        a, b = self._ab(self.mm(uv))
        return self.centre + a[:, None] * self.r + b[:, None] * self.up

    def flat_px(self, mm):
        return (np.atleast_2d(np.asarray(mm, float)) + self.margin) * self.ppm

    def corners_flat(self):
        x0, y0, x1, y1 = self.rect_mm
        return self.flat_px([[x0, y0], [x1, y0], [x1, y1], [x0, y1]])


def faces():
    import cabinet0096_trace as C
    import sp2m_trace as S
    cab = V.load_cabin()
    out = {}
    p = cab['panels']['sp2m_panel']
    out['sp2m'] = Face('sp2m', os.path.join(REPO, 'out', 'traces', 'sp2m', 'front_mm.png'), 2.0, 20.0, p,
                       lambda m: ((m[:, 0] - S.W_MM / 2) * 0.001, (S.H_MM / 2 - m[:, 1]) * 0.001), (0, 0, S.W_MM, S.H_MM), 'g093-b')
    p = cab['panels']['pwr_cabinet']
    d = C.DOOR
    s = C.DOOR_PROUD / -C.CAM_A[2]

    def cab_ab(m):                       # a reading on g096-a's flat of something on the door: back on the door (the trace's `fix`), about the door's centre, at the game's scale
        x = m[:, 0] * (1 - s) + C.CAM_A[0] * s
        y = m[:, 1] * (1 - s) + C.CAM_A[1] * s
        return (x - 0.5 * (d[0] + d[2])) * 0.001 * C.SCALE, (0.5 * (d[1] + d[3]) - y) * 0.001 * C.SCALE
    out['cab0096'] = Face('cab0096', os.path.join(REPO, 'out', 'traces', 'cabinet0096_g096a', 'front_mm.png'), 4.0, 40.0, p, cab_ab, (28.0, 10.0, 653.5, 680.0), 'g096-a')
    return out


_MATCH = {}


def matcher():
    """ALIKED + LightGlue, the pair the joint solve used."""
    if not _MATCH:
        import torch
        from lightglue import ALIKED, LightGlue
        _MATCH['e'] = ALIKED(max_num_keypoints=4096, detection_threshold=0.005).eval().cuda()
        _MATCH['m'] = LightGlue(features='aliked', filter_threshold=0.1).eval().cuda()
        _MATCH['t'] = torch
    return _MATCH


def match(a_bgr, b_bgr):
    """Matched pixels of two images (N x 2 each)."""
    from lightglue.utils import numpy_image_to_torch, rbd
    m = matcher()
    t = m['t']
    with t.inference_mode():
        fa = m['e'].extract(numpy_image_to_torch(cv2.cvtColor(a_bgr, cv2.COLOR_BGR2RGB)).cuda(), resize=None)
        fb = m['e'].extract(numpy_image_to_torch(cv2.cvtColor(b_bgr, cv2.COLOR_BGR2RGB)).cuda(), resize=None)
        r = rbd(m['m']({'image0': fa, 'image1': fb}))
        fa, fb = rbd(fa), rbd(fb)
    k = r['matches'].cpu().numpy()
    return fa['keypoints'].cpu().numpy()[k[:, 0]], fb['keypoints'].cpu().numpy()[k[:, 1]]
