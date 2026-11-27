"""Stage 6 of docs/OSA_MODELLING_PROCESS.md: acceptance per part and per view, in pixels of outline.

For every part of data/cabins/osa_ak_parts.json and every photo that has evidence for it: the photo's measured outline
against the part's same-named edge through that photo's certified camera. A part fitted from one photo only is also
checked in others: its place there, through the camera, prompts SAM 2 (a prompt, not a measurement), and the mask's
outline is compared with the part's.

    python tools/recon/osa/accept.py [part ...] [--write]     # --write stores the figures in the parts file as accepted

Out: out/recon/osa/accept/<part>_<photo>.jpg (photo's outline green, the part yellow, the model as it was red), accept.json.
"""
import os
import sys

import cv2
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import core as C  # noqa: E402
import evidence as E  # noqa: E402
import fit as F  # noqa: E402
import store  # noqa: E402

OUT = os.path.join(C.OUT, 'accept')
GATE_PX, GATE_SHARE = 4.0, 0.0025       # a part passes in a view at the larger of these (the share is of the photo's width), the worst tenth at three times that


def gate(cam):
    return max(GATE_PX, GATE_SHARE * cam.w)


def model_line(p, shape, feature):
    """The part's edge that an evidence feature names: a stored measured line, or one of the shape's feature curves."""
    if feature in p.get('feature_lines_m', {}):
        return np.array(p['feature_lines_m'][feature], float), False
    return shape.features()[p['evidence_features'][feature]]


def before_lines(pid):
    """The same edge as the model had it before the rebuild (data/cabins/osa_ak_shell.json), for the overlays: [(3-D line)]."""
    sh = C.load(os.path.join(C.REPO, 'data', 'cabins', 'osa_ak_shell.json'))['posts']['right']
    if pid == 'housing.right' and 'shroud' in sh:
        return [np.array([(x, sh['shroud']['y_m'], z) for x, z in sh['shroud']['outline_x_z_m']], float)]
    if pid == 'post.right':
        return [np.array([(x, sh['leg_y_m'][1] if z < sh['knee_z_m'] else sh['y_m'][1], z) for x, z in sh['x_z_m']], float)]
    return []


def old_camera(view, w, h):
    """The camera the last session judged through (holistic_diff's), for drawing the model as it was judged."""
    p = os.path.join(C.REPO, 'out', 'recon', 'holistic', 'diff', 'ref', view + '.npz')
    if not os.path.exists(p):
        return None
    z = np.load(p)
    return C.Camera(float(z['f']), float(z['cx']), float(z['cy']), z['R'], z['t'], int(z['w']), int(z['h']), float(z['k1'])).scaled(w, h)


def dense(X, step=0.004):
    out = [X[:1]]
    for a, b in zip(X[:-1], X[1:]):
        n = max(1, int(np.linalg.norm(b - a) / step))
        out.append(a + (b - a) * np.linspace(0, 1, n + 1)[1:, None])
    return np.vstack(out)


def check_by_projection(shape, view, cam):
    """A part with no evidence in this photo: SAM 2 prompted at where the camera puts the part's middle; the mask's outline against the part's."""
    V, _ = shape.mesh()
    c = C.project(cam, V.mean(0))[0]
    if not np.isfinite(c).all() or not (0 < c[0] < cam.w and 0 < c[1] < cam.h):
        return None
    m, score = E.prompt_mask(view, [c.tolist()])
    return E.resample(E.mask_outline(m)), score


def run(only=None, write=False):
    doc = store.load()
    cams = C.cameras()
    os.makedirs(OUT, exist_ok=True)
    report = {}
    for p in doc['parts']:
        if only and p['id'] not in only:
            continue
        shape = store.shape(p)
        rows = {}
        views = E.views_of(p['id'])
        for v in sorted(cams):
            if cams[v]['grade'] != 'A':
                continue
            cam = cams[v]['camera']
            lines, checked = [], False
            for feat in p['evidence_features']:
                for it in E.items(v, p['id'], feat):
                    X, closed = model_line(p, shape, feat)
                    if p['evidence_features'][feat] == 'hull':          # a cloud of points whose silhouette is the part's outline
                        e = F.outline_error(cam, X, it['pieces'], hull=True)
                        q = C.project(cam, X)
                        X = None, cv2.convexHull(q[np.isfinite(q).all(1)].astype(np.float32)).reshape(-1, 2)
                    else:
                        e = F.outline_error(cam, dense(np.vstack([X, X[:1]]) if closed else X), it['pieces'])
                    lines.append((feat, it['pieces'], X, closed, e))
            if not lines and len(views) < 2 and v in p.get('check_views', []):
                got = check_by_projection(shape, v, cam)
                if got:
                    X, closed = model_line(p, shape, next(iter(p['evidence_features'])))
                    e = F.outline_error(cam, dense(np.vstack([X, X[:1]]) if closed else X), [got[0]])
                    lines.append(('check', [got[0]], X, closed, e))
                    checked = True
            if not lines:
                continue
            e = np.concatenate([q[4] for q in lines])
            e = e[np.isfinite(e)]
            g = gate(cam)
            middle = shape.mesh()[0].mean(0)
            row = {'median_px': round(float(np.median(e)), 1), 'p90_px': round(float(np.percentile(e, 90)), 1), 'gate_px': round(g, 1), 'passes': bool(np.median(e) <= g and np.percentile(e, 90) <= 3 * g), 'share_of_width': round(float(np.median(e)) / cam.w, 4),
                   'mm_at_part': round(float(np.median(e)) * float(np.linalg.norm(middle - cam.centre())) / cam.f * 1000, 1), 'how': 'checked by a prompted mask' if checked else 'evidence'}
            img = C.photo(v)
            old = old_camera(v, cam.w, cam.h)
            before = before_lines(p['id'])
            if old is not None and before:
                eb = []
                for B in before:
                    q = C.project(old, dense(B))
                    q = q[np.isfinite(q).all(1)]
                    cv2.polylines(img, [np.round(q).astype(np.int32)], False, (0, 0, 255), 2, cv2.LINE_AA)
                    eb.append(F.seg_dist(np.vstack([np.vstack(l[1]) for l in lines]), q))
                eb = np.min(eb, axis=0)
                row['before_median_px'], row['before_p90_px'] = round(float(np.median(eb)), 1), round(float(np.percentile(eb, 90)), 1)
            pts = []
            for feat, pieces, X, closed, _ in lines:
                for q in pieces:
                    cv2.polylines(img, [np.round(q).astype(np.int32)], False, (0, 255, 0), 2, cv2.LINE_AA)
                    pts.append(q)
            Vm, Fm = shape.mesh()
            pm = C.project(cam, Vm)
            for a, b, c_ in Fm[::1]:
                tri = pm[[a, b, c_]]
                if np.isfinite(tri).all():
                    cv2.polylines(img, [np.round(tri).astype(np.int32)], True, (0, 150, 200), 1, cv2.LINE_AA)
            for feat, pieces, X, closed, _ in lines:
                if isinstance(X, tuple):                                # a silhouette already in pixels
                    cv2.polylines(img, [np.round(X[1]).astype(np.int32)], True, (0, 230, 255), 2, cv2.LINE_AA)
                    continue
                q = C.project(cam, dense(np.vstack([X, X[:1]]) if closed else X))
                cv2.polylines(img, [np.round(q[np.isfinite(q).all(1)]).astype(np.int32)], False, (0, 230, 255), 2, cv2.LINE_AA)
            pts = np.vstack(pts)
            pad = 260
            x0, x1, y0, y1 = int(max(0, pts[:, 0].min() - pad)), int(min(cam.w, pts[:, 0].max() + pad)), int(max(0, pts[:, 1].min() - pad)), int(min(cam.h, pts[:, 1].max() + pad))
            crop = np.ascontiguousarray(img[y0:y1, x0:x1])
            txt = '%s in %s: %.1f px (worst tenth %.1f)%s' % (p['id'], v, row['median_px'], row['p90_px'], ('; as it was %.1f (%.1f)' % (row['before_median_px'], row['before_p90_px'])) if 'before_median_px' in row else '')
            cv2.putText(crop, txt, (8, 20), 0, 0.5, (0, 0, 0), 4, cv2.LINE_AA)
            cv2.putText(crop, txt, (8, 20), 0, 0.5, (255, 255, 255), 1, cv2.LINE_AA)
            cv2.imwrite(os.path.join(OUT, '%s_%s.jpg' % (p['id'], v)), crop, [cv2.IMWRITE_JPEG_QUALITY, 92])
            rows[v] = row
            print('%-20s %-7s %5.1f px median, %5.1f worst tenth (gate %.1f) %s  %.1f mm%s   [%s]' % (p['id'], v, row['median_px'], row['p90_px'], g, 'PASS' if row['passes'] else 'fail', row['mm_at_part'],
                                                                                                    ('   as it was: %.1f, %.1f' % (row['before_median_px'], row['before_p90_px'])) if 'before_median_px' in row else '', row['how']), flush=True)
        report[p['id']] = rows
        if write:
            p['accepted'] = {v: {k: r[k] for k in ('median_px', 'p90_px', 'gate_px', 'passes', 'how')} for v, r in rows.items()}
    C.save(os.path.join(OUT, 'accept.json'), report)
    if write:
        C.save(C.PARTS, doc, indent=None)
    return report


if __name__ == '__main__':
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    run(args or None, '--write' in sys.argv)
