"""How far each part of the assembled interior must move to sit on the photographs: from panel_score.py's renders, the 2D
shift of each part's model edges that best lands them on the photo's edges in each trusted view, then one 3D move per
part that explains those shifts through the grade-A cameras (least squares on each camera's projection at the part's
centre, the moves along each view's line of sight left free unless two views pin them).

    python tools/recon/osa/panel_fit.py [PART ...] [--search 40] [--min-gain 0.15]

A view counts for a part only when the shift gains: the share of the part's model edges within 3 px of a photo edge
rises by --min-gain or more (a part hidden in the photo, by a folded seat say, gains nothing and is left out). Writes
out/recon/osa/panel_score/fit.json: per part, each view's shift (px), its gain, and the 3D move (m, body frame) with
its residual (px); the move is reported, never applied: the trace or the cabin file is edited by hand from it.
"""
import argparse
import json
import os
import sys

import cv2
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(REPO, 'tools', 'recon'))
import holistic_view as V  # noqa: E402
import panel_score as PS  # noqa: E402


def best_shift(pts, dt, search):
    """The integer shift (dx, dy) within +-search px minimising the mean clipped distance of pts to the photo's edges."""
    h, w = dt.shape
    clip = np.minimum(dt, 12.0)

    def cost(dx, dy):
        x, y = pts[:, 0] + dx, pts[:, 1] + dy
        k = (x >= 0) & (x < w) & (y >= 0) & (y < h)
        if k.mean() < 0.8:
            return 1e9
        return float(clip[y[k], x[k]].mean())

    best = (cost(0, 0), 0, 0)
    for step in (4, 1):
        cx, cy = best[1], best[2]
        rng = range(-search, search + 1, step) if step == 4 else range(-4, 5)
        for dy in rng:
            for dx in rng:
                sx, sy = (dx, dy) if step == 4 else (cx + dx, cy + dy)
                c = cost(sx, sy)
                if c < best[0]:
                    best = (c, sx, sy)
    return best[1], best[2]


def within3(pts, dt, dx, dy):
    h, w = dt.shape
    x, y = pts[:, 0] + dx, pts[:, 1] + dy
    k = (x >= 0) & (x < w) & (y >= 0) & (y < h)
    return float((dt[y[k], x[k]] <= 3).mean()) if k.any() else 0.0


def jacobian(cam, c):
    """d(pixel)/d(body metres) at c, 2x3."""
    J = np.zeros((2, 3))
    p0 = cam.project(c[None])[0]
    for i in range(3):
        e = np.zeros(3)
        e[i] = 0.01
        J[:, i] = (cam.project((c + e)[None])[0] - p0) / 0.01
    return J


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('parts', nargs='*')
    ap.add_argument('--search', type=int, default=40)
    ap.add_argument('--min-gain', type=float, default=0.15)
    a = ap.parse_args(argv)
    score = V.load(os.path.join(PS.OUT, 'score.json'))
    width = score['width']
    centres = PS.part_centres()
    per_view = {}
    for r in score['rows']:
        if a.parts and r['part'] not in a.parts:
            continue
        per_view.setdefault(r['view'], []).append(r['part'])
    obs = {}
    for view, parts in per_view.items():
        cam = PS.certified(view, width)
        prefix = os.path.join(PS.OUT, view + '_')
        photo = cv2.resize(cv2.imread(V.photo_file(view, 'clean')), (cam.w, cam.h), interpolation=cv2.INTER_AREA)
        dt = cv2.distanceTransform(255 - PS.edges(photo), cv2.DIST_L2, 3)
        em = PS.edges(cv2.imread(prefix + 'model.png'))
        ids = cv2.cvtColor(cv2.imread(prefix + 'id.png'), cv2.COLOR_BGR2RGB)
        by_part = json.load(open(prefix + 'id.json', encoding='utf-8'))['by_part']
        for part in parts:
            mask = np.all(ids == np.array(by_part[part], np.uint8), axis=-1).astype(np.uint8)
            ys, xs = np.nonzero((em > 0) & (cv2.dilate(mask, np.ones((3, 3), np.uint8)) > 0))
            pts = np.stack([xs, ys], 1)
            dx, dy = best_shift(pts, dt, a.search)
            w0, w1 = within3(pts, dt, 0, 0), within3(pts, dt, dx, dy)
            o = {'view': view, 'shift_px': [dx, dy], 'within3': [round(w0, 3), round(w1, 3)], 'used': w1 - w0 >= a.min_gain}
            if part in centres:
                o['J'] = jacobian(cam, centres[part]).tolist()
            obs.setdefault(part, []).append(o)
    out = {}
    for part, os_ in sorted(obs.items()):
        used = [o for o in os_ if o['used'] and 'J' in o]
        res = {'views': os_}
        if used:
            J = np.vstack([np.array(o['J']) for o in used])
            s = np.concatenate([np.array(o['shift_px'], float) for o in used])
            lam = 50.0 if len(used) == 1 else 5.0        # damping: a single view cannot see along its own line of sight
            d = np.linalg.solve(J.T @ J + lam * np.eye(3) * (np.trace(J.T @ J) / 3e4), J.T @ s)
            res['move_m'] = [round(float(v), 4) for v in d]
            res['residual_px'] = round(float(np.sqrt(np.mean((J @ d - s) ** 2))), 2)
            res['n_views'] = len(used)
        out[part] = res
        mv = res.get('move_m')
        print('%-22s %s %s' % (part, ('move %+.3f %+.3f %+.3f m  resid %.1f px  views %d' % (*mv, res['residual_px'], res['n_views'])) if mv else 'no gain anywhere',
                                ' '.join('%s:(%+d,%+d %.2f->%.2f%s)' % (o['view'], *o['shift_px'], *o['within3'], '' if o['used'] else ' x') for o in os_)))
    V.save(os.path.join(PS.OUT, 'fit.json'), out)
    return 0


if __name__ == '__main__':
    sys.exit(main())
