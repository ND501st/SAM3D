"""Fit a vehicle's main dimensions and a photo's camera together (docs/MODELLING_FROM_PHOTOS.md, for vehicles).

A side photograph taken close and wide shows perspective in depth as well as along the hull, so reading lengths off it
in 2-D (tools/vehicle_trace.py) leaves errors of a few per cent. Here the hull is a handful of parameters (wheelbase,
outer axle spacing, overhangs, the heights of the bow's edge and the stern box, the widths at the ends and the roof, the
tyre's radius, where the roof starts and ends), the camera is a pinhole (focal length, pose), and both are fitted to the
points read off the photo, with the published dimensions as priors: tight where a source is sure (the length, the
roof's height, the track), loose where the value is SAM3D's reading.

    python tools/vehicle_fit.py tools/traces/<name>.json              # prints the fitted dimensions and each point's residual
    python tools/vehicle_fit.py tools/traces/<name>.json backproject  # the config's "backproject" pixels onto their planes

Config: "photo_size", "fit": {"points": {name: [x_px, y_px]}, "priors": {param: [value, sigma]}, "fixed": {...},
"start": {...}}; a prior "f_px" holds the focal length (from the photo's EXIF: 35 mm-equivalent focal / 36 x the
width in px). The points it knows: hub1..4, ground1..4 (rear to front), bow_corner, stern_top, stern_bottom, roof_rear,
roof_front. Frame: X forward from the middle of the wheelbase, Y left, Z up; the photo shows the right side.
"""
import json
import math
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

PARAMS = ['W', 's', 'Lr', 'Lf', 'z_bow', 'z_st', 'z_sb', 'roof_hw', 'bow_hw', 'stern_hw', 'tyre_r', 'x_rr', 'x_rf']


def points3d(p, fixed):
    """Trace-frame 3-D points for the parameters."""
    W, a = p['W'], p['s'] * p['W']
    axles = [-W / 2, -W / 2 + a, W / 2 - a, W / 2]
    hub_y = -(fixed['track'] / 2 + fixed['tyre_width'] / 2)
    out = {}
    for i, x in enumerate(axles):
        out['hub%d' % (i + 1)] = (x, hub_y, p['tyre_r'])
        out['ground%d' % (i + 1)] = (x, hub_y, 0.0)
    out['bow_corner'] = (W / 2 + p['Lf'], -p['bow_hw'], p['z_bow'])
    out['stern_top'] = (-W / 2 - p['Lr'], -p['stern_hw'], p['z_st'])
    out['stern_bottom'] = (-W / 2 - p['Lr'], -p['stern_hw'], p['z_sb'])
    out['roof_rear'] = (p['x_rr'], -p['roof_hw'], fixed['roof_z'])
    out['roof_front'] = (p['x_rf'], -p['roof_hw'], fixed['roof_z'])
    return out


def residuals(vec, names, cfg, cam):
    import cv2
    import numpy as np
    p = dict(zip(PARAMS, vec[:len(PARAMS)]))
    f = vec[len(PARAMS)]
    rv = np.array(vec[len(PARAMS) + 1:len(PARAMS) + 4], float)
    tv = np.array(vec[len(PARAMS) + 4:len(PARAMS) + 7], float)
    w, h = cfg['photo_size']
    K = np.array([[f, 0, w / 2], [0, f, h / 2], [0, 0, 1]], float)
    P = points3d(p, cfg['fit']['fixed'])
    obj = np.array([[P[n][1], -P[n][0], P[n][2]] for n in names], float)  # Blender: X = left, Y = back
    proj, _ = cv2.projectPoints(obj, rv, tv, K, np.zeros(5))
    img = np.array([cfg['fit']['points'][n] for n in names], float)
    sigma_px = cfg['fit'].get('sigma_px', 4.0)
    r = list(((proj.reshape(-1, 2) - img) / sigma_px).ravel())
    for k, (val, sig) in cfg['fit']['priors'].items():
        v = p['W'] + p['Lr'] + p['Lf'] if k == 'length' else f if k == 'f_px' else p[k]
        r.append((v - val) / sig)
    return np.array(r)


def solve(cfg):
    import cv2
    import numpy as np
    fit = cfg['fit']
    names = list(fit['points'])
    start = dict(fit['start'])
    # the camera's start from a pose solve on the start's points
    w, h = cfg['photo_size']
    P = points3d(start, fit['fixed'])
    obj = np.array([[P[n][1], -P[n][0], P[n][2]] for n in names], np.float32)
    img = np.array([fit['points'][n] for n in names], np.float32)
    K0 = np.array([[2.0 * w, 0, w / 2], [0, 2.0 * w, h / 2], [0, 0, 1]], np.float64)
    flags = (cv2.CALIB_USE_INTRINSIC_GUESS | cv2.CALIB_FIX_PRINCIPAL_POINT | cv2.CALIB_FIX_ASPECT_RATIO |
             cv2.CALIB_ZERO_TANGENT_DIST | cv2.CALIB_FIX_K1 | cv2.CALIB_FIX_K2 | cv2.CALIB_FIX_K3)
    _, K, _, rv, tv = cv2.calibrateCamera([obj], [img], (w, h), K0, np.zeros(5), flags=flags)
    f0 = fit['priors']['f_px'][0] if 'f_px' in fit['priors'] else K[0, 0]
    vec = np.array([start[k] for k in PARAMS] + [f0] + list(rv[0].ravel()) + list(tv[0].ravel()), float)
    lam = 1e-2
    r = residuals(vec, names, cfg, None)
    for _ in range(300):
        J = np.zeros((len(r), len(vec)))
        for j in range(len(vec)):
            d = np.zeros(len(vec))
            d[j] = 1e-5 * max(1.0, abs(vec[j]))
            J[:, j] = (residuals(vec + d, names, cfg, None) - r) / d[j]
        A = J.T @ J
        g = J.T @ r
        step = np.linalg.solve(A + lam * np.diag(np.diag(A) + 1e-9), -g)
        r2 = residuals(vec + step, names, cfg, None)
        if r2 @ r2 < r @ r:
            vec, r, lam = vec + step, r2, lam * 0.3
            if np.linalg.norm(step) < 1e-9:
                break
        else:
            lam *= 10.0
            if lam > 1e12:
                break
    p = dict(zip(PARAMS, vec[:len(PARAMS)]))
    sigma_px = fit.get('sigma_px', 4.0)
    img_res = r[:2 * len(names)].reshape(-1, 2) * sigma_px
    rv, tv = vec[len(PARAMS) + 1:len(PARAMS) + 4], vec[len(PARAMS) + 4:]
    R, _ = cv2.Rodrigues(np.array(rv))
    C = -R.T @ np.array(tv)
    return p, float(vec[len(PARAMS)]), R, np.array(tv), C, dict(zip(names, np.linalg.norm(img_res, axis=1)))


def main():
    cfg = json.load(open(sys.argv[1], encoding='utf-8'))
    p, f, R, t, C, res = solve(cfg)
    rms = math.sqrt(sum(v * v for v in res.values()) / len(res))
    print('rms %.2f px over %d points; f %.0f px; camera at %s (Blender m)' % (rms, len(res), f, [round(float(v), 2) for v in C]))
    for k in PARAMS:
        print('  %-9s %.3f' % (k, p[k]))
    print('  length    %.3f' % (p['W'] + p['Lr'] + p['Lf']))
    print('  axles     %s' % [round(v, 3) for v in (-p['W'] / 2, -p['W'] / 2 + p['s'] * p['W'], p['W'] / 2 - p['s'] * p['W'], p['W'] / 2)])
    for k, v in res.items():
        print('    %-13s %.1f px' % (k, v))
    folder = os.path.join(REPO, 'out', 'traces', cfg['name'])
    os.makedirs(folder, exist_ok=True)
    w, h = cfg['photo_size']
    json.dump({'f_px': f, 'w': w, 'h': h, 'R': R.tolist(), 't': t.tolist(), 'rms_px': rms, 'frame': 'world: Blender metres',
               'params': p}, open(os.path.join(folder, 'camera_world.json'), 'w'), indent=1)


if __name__ == '__main__' and len(sys.argv) == 2:
    main()


def backproject(cam, px, y_plane):
    """The trace-frame (X, Z) where the camera's ray through pixel `px` meets the vertical plane Y = y_plane (the side's
    plane a feature lies in)."""
    import numpy as np
    R = np.array(cam['R'])
    t = np.array(cam['t'])
    f, w, h = cam['f_px'], cam['w'], cam['h']
    d_cam = np.array([(px[0] - w / 2) / f, (px[1] - h / 2) / f, 1.0])
    C = -R.T @ t                      # camera centre, Blender frame
    d = R.T @ d_cam                   # ray direction, Blender frame
    bx = y_plane                      # Blender X = trace Y
    s = (bx - C[0]) / d[0]
    P = C + s * d
    return float(-P[1]), float(P[2])  # trace X = -Blender Y; Z


def cmd_backproject(cfg_path):
    cfg = json.load(open(cfg_path, encoding='utf-8'))
    cam = json.load(open(os.path.join(REPO, 'out', 'traces', cfg['name'], 'camera_world.json')))
    for name, v in cfg.get('backproject', {}).items():
        x, z = backproject(cam, v['px'], v['y'])
        print('  %-18s X %7.3f  Z %6.3f   (plane Y %.3f)' % (name, x, z, v['y']))


if __name__ == '__main__' and len(sys.argv) > 2 and sys.argv[2] == 'backproject':
    cmd_backproject(sys.argv[1])
