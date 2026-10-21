"""tools/vehicle_fit.py recovers a vehicle's dimensions and the camera from a synthetic photograph made with known ones,
and back-projects a pixel onto the side plane it came from."""
import sys
from pathlib import Path

import pytest

pytest.importorskip('cv2')  # OpenCV, as tools/photo_trace.py
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import vehicle_fit as F  # noqa: E402

TRUTH = {'W': 4.4, 's': 0.317, 'Lr': 1.29, 'Lf': 1.53, 'z_bow': 1.6, 'z_st': 1.32, 'z_sb': 0.83, 'roof_hw': 1.06,
         'bow_hw': 0.65, 'stern_hw': 0.975, 'tyre_r': 0.55, 'x_rr': -2.55, 'x_rf': 2.3}
FIXED = {'track': 2.38, 'tyre_width': 0.33, 'roof_z': 2.055}


def synthetic_photo():
    import cv2
    import numpy as np
    w, h, f = 1800, 1200, 1350.0
    K = np.array([[f, 0, w / 2], [0, f, h / 2], [0, 0, 1.0]])
    # a camera 7.5 m off the right side, a little forward, 1.2 m up, looking at the hull
    C = np.array([-7.5, -0.4, 1.2])
    target = np.array([0.0, 0.0, 1.1])
    z = target - C
    z /= np.linalg.norm(z)
    x = np.cross(z, [0, 0, 1.0])
    x /= np.linalg.norm(x)
    y = np.cross(z, x)
    R = np.array([x, y, z])
    t = -R @ C
    rv, _ = cv2.Rodrigues(R)
    P = F.points3d(TRUTH, FIXED)
    names = [n for n in P if n != 'roof_front']
    obj = np.array([[P[n][1], -P[n][0], P[n][2]] for n in names], float)
    proj, _ = cv2.projectPoints(obj, rv, t, K, np.zeros(5))
    pts = {n: [float(u), float(v)] for n, (u, v) in zip(names, proj.reshape(-1, 2))}
    return {'name': 'test_vehicle_fit', 'photo_size': [w, h],
            'fit': {'sigma_px': 4.0, 'points': pts, 'fixed': FIXED,
                    'priors': {'length': [7.22, 0.01], 'f_px': [1350.0, 20.0], 'roof_hw': [1.06, 0.04], 'bow_hw': [0.65, 0.04],
                               'stern_hw': [0.975, 0.04], 'tyre_r': [0.55, 0.03], 'W': [4.4, 0.4]},
                    'start': dict(TRUTH, W=4.1, Lr=1.5, Lf=1.6, z_bow=1.45, tyre_r=0.52)}}, f, R, t


def test_the_fit_recovers_the_dimensions_and_the_camera():
    cfg, f, R, t = synthetic_photo()
    p, f_fit, R_fit, t_fit, C, res = F.solve(cfg)
    assert max(res.values()) < 0.5  # px
    for k in ('W', 'Lr', 'Lf', 'z_bow', 'z_st', 'z_sb', 'tyre_r'):
        assert abs(p[k] - TRUTH[k]) < 0.02, (k, p[k])
    assert abs(f_fit - f) < 10.0


def test_backproject_finds_the_point_on_its_plane():
    import numpy as np
    cfg, f, R, t = synthetic_photo()
    cam = {'R': R.tolist(), 't': list(t), 'f_px': f, 'w': 1800, 'h': 1200}
    # the chine at the middle of the hull's right side
    X, Y, Z = -0.1, -1.4125, 1.39
    Pc = R @ np.array([Y, -X, Z]) + t
    px = [f * Pc[0] / Pc[2] + 900, f * Pc[1] / Pc[2] + 600]
    x, z = F.backproject(cam, px, Y)
    assert abs(x - X) < 1e-6 and abs(z - Z) < 1e-6
