"""Read a vehicle's side profile off a side photograph into metres (docs/MODELLING_FROM_PHOTOS.md, adapted to vehicles).

A vehicle's side is not one plane and a museum photo is seldom square-on: the camera stands off to one end and far away,
so the hull's long lines stay almost parallel (a homography sees no perspective) while the length is foreshortened by the
cosine of the yaw. The flattening is therefore done along the side instead of in a plane:

* x (along the hull): the wheel hubs stand in one plane parallel to the hull's axis, at known spacings (the published
  wheelbase, the outer and middle spacings fitted). A 1-D projective map x_px = (A X + B) / (C X + 1) through them takes
  out both the foreshortening and the perspective along the length.
* z (heights): at each X the vertical scale is the horizontal one divided by cos(yaw); cos(yaw) comes from one published
  height (the roof's) and the ground line under the hubs.
* Points on the side plates (the chine, hatches, the plate edges) stand at about the hubs' lateral offset, so they map
  with the same formula; the bow tip and the stern's corner (on or near the centreline) are shifted by their offset times
  sin(yaw), given in the config per point ("dy_m": how far nearer the centreline than the hubs, metres). Seen from the
  front quarter, such a point shows further toward the bow, so its X comes back by dy tan(yaw).

    python tools/vehicle_trace.py tools/traces/<name>.json            # prints every point in metres, writes out/traces/<name>/points.json

Config: {"name", "photo", "axles_m": total wheelbase, "hubs_px": [[x, y], ...] rear to front, "symmetric_axles": true
(outer spacings equal), "ground_px": [[x, y], [x, y]] (two points on the ground line under the wheels), "known_height":
{"point": name, "m": metres}, "points": {name: [x_px, y_px] or {"px": [x, y], "dy_m": d}}}. X is from the rear axle,
forward; Z up from the ground.
"""
import json
import math
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def solve_axles(hubs_px, wheelbase_m, symmetric=True):
    """The 1-D projective map through the hubs: returns (A, B, C, axle X positions)."""
    import numpy as np
    xs = [h[0] for h in hubs_px]
    n = len(xs)

    def model(params):
        A, B, C, a = params
        if n == 4 and symmetric:
            X = [0.0, a, wheelbase_m - a, wheelbase_m]
        else:
            X = [wheelbase_m * k / (n - 1) for k in range(n)]
        return X, [(A * x + B) / (C * x + 1.0) for x in X]

    # Gauss-Newton from a linear start.
    k0 = (xs[-1] - xs[0]) / wheelbase_m
    p = np.array([k0, xs[0], 0.0, wheelbase_m * 0.31])
    for _ in range(60):
        X, pred = model(p)
        r = np.array(pred) - np.array(xs)
        J = np.zeros((n, 4))
        for j in range(4):
            d = np.zeros(4)
            d[j] = 1e-6 * max(1.0, abs(p[j]))
            _, pd = model(p + d)
            J[:, j] = (np.array(pd) - np.array(pred)) / d[j]
        step = np.linalg.lstsq(J, -r, rcond=None)[0]
        p = p + step
        if np.linalg.norm(step) < 1e-10:
            break
    X, pred = model(p)
    return float(p[0]), float(p[1]), float(p[2]), X, [float(v - x) for v, x in zip(pred, xs)]


def run(cfg):
    A, B, C, axles, resid = solve_axles(cfg['hubs_px'], cfg['axles_m'], cfg.get('symmetric_axles', True))
    (gx0, gy0), (gx1, gy1) = cfg['ground_px']

    def X_of(px):
        return (px - B) / (A - C * px)

    def kx(X):  # px per metre along the hull at X (foreshortened)
        return (A - C * B) / (C * X + 1.0) ** 2

    def ground_y(px):
        return gy0 + (gy1 - gy0) * (px - gx0) / (gx1 - gx0)

    kh = cfg['known_height']
    p0 = cfg['points'][kh['point']]
    p0 = p0['px'] if isinstance(p0, dict) else p0
    X0 = X_of(p0[0])
    cos_yaw = kx(X0) * kh['m'] / (ground_y(p0[0]) - p0[1])
    cos_yaw = min(cos_yaw, 1.0)
    yaw = math.acos(cos_yaw)
    out = {'axles_m': axles, 'hub_residuals_px': resid, 'yaw_deg': math.degrees(yaw), 'points': {}}
    for name, v in cfg['points'].items():
        px, dy = (v['px'], v.get('dy_m', 0.0)) if isinstance(v, dict) else (v, 0.0)
        X = X_of(px[0])
        kz = kx(X) / cos_yaw
        Z = (ground_y(px[0]) - px[1]) / kz
        X -= dy * math.tan(yaw)  # nearer the centreline: from the front quarter it shows further toward the bow
        out['points'][name] = [round(X, 3), round(Z, 3)]
    return out


def main():
    cfg = json.load(open(sys.argv[1], encoding='utf-8'))
    out = run(cfg)
    folder = os.path.join(REPO, 'out', 'traces', cfg['name'])
    os.makedirs(folder, exist_ok=True)
    json.dump(out, open(os.path.join(folder, 'points.json'), 'w'), indent=1)
    print('axles (m from the rear axle):', [round(a, 3) for a in out['axles_m']], ' hub residuals px:',
          [round(r, 2) for r in out['hub_residuals_px']], ' yaw %.1f deg' % out['yaw_deg'])
    for k, (x, z) in out['points'].items():
        print('  %-18s X %7.3f  Z %6.3f' % (k, x, z))


if __name__ == '__main__':
    main()
