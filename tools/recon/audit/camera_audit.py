"""Camera audit (handoff section 8.3): the faces traced in millimetres, as the cabin places them, projected through each
view's camera, against where the photo has them.

For a face with a millimetre flat (SP-2M2 from g093-b, the 0096-9M2's door from g096-a), ALIKED + LightGlue match the
flat to the photo: directly at three sizes, and warped into the view by the view's own camera (which finds a face seen
at a slant, when the camera is near enough right). Each match is a point of the face whose place in the cabin is known
and whose pixel in the photo is measured. A homography (RANSAC) keeps only the matches lying on the face.

    error under the pipeline's camera = measured pixel - the point's projection through holistic_diff's registered camera

Then the same matches re-solve the camera from nothing (planar PnP at the registered focal length, then pose and focal
length together), which says where the camera is by the traced face alone, and how far the pipeline's camera is from it.

    python tools/recon/audit/camera_audit.py [photo ...]

Out: out/recon/audit/camera/<photo>.jpg (the face's outline as the pipeline's camera projects it, cyan; as the resection
projects it, green; each match's error drawn from projected to measured, red), camera.json.
"""
import json
import os
import sys

import cv2
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as A  # noqa: E402

OUT = os.path.join(A.OUT, 'camera')
MIN_ON_FACE = 40


def prewarp(face, cam, flat, w, h):
    """The flat as the camera would see the face, or None when it cannot."""
    if np.dot(face.n, cam.centre() - face.centre) <= 0.05:
        return None
    x0, y0, x1, y1 = face.rect_mm
    gx, gy = np.meshgrid(np.linspace(x0, x1, 9), np.linspace(y0, y1, 9))
    fl = face.flat_px(np.stack([gx.ravel(), gy.ravel()], 1))
    px = A.project(cam, face.body(fl))
    ok = np.isfinite(px).all(1)
    inside = ok & (px[:, 0] > 0) & (px[:, 0] < w) & (px[:, 1] > 0) & (px[:, 1] < h)
    if ok.sum() < 20 or inside.sum() < 6 or np.ptp(px[inside], axis=0).max() < 120:
        return None
    H, _ = cv2.findHomography(fl[ok], px[ok], 0)
    warped = cv2.warpPerspective(flat, H, (w, h), flags=cv2.INTER_AREA)
    m = cv2.warpPerspective(np.full(flat.shape[:2], 255, np.uint8), H, (w, h)) > 0
    ys, xs = np.nonzero(m)
    if len(xs) < 5000:
        return None
    pad = 60
    return H, warped, (max(xs.min() - pad, 0), min(xs.max() + pad, w), max(ys.min() - pad, 0), min(ys.max() + pad, h))


def find_face(face, flat, img, cam):
    """Matches between the flat (its pixels) and the photo that lie on the face: the best of the direct tries and the pre-warped one."""
    h, w = img.shape[:2]
    best = (0, None, None, '')
    tries = []
    for width in (1800, 1100, 650):
        s = width / flat.shape[1]
        a, b = A.match(cv2.resize(flat, None, fx=s, fy=s, interpolation=cv2.INTER_AREA), img)
        tries.append(('direct %d' % width, a / s, b))
    pw = prewarp(face, cam, flat, w, h)
    if pw is not None:
        H, warped, (x0, x1, y0, y1) = pw
        a, b = A.match(warped[y0:y1, x0:x1], img[y0:y1, x0:x1])
        if len(a):
            a = cv2.perspectiveTransform((a + [x0, y0]).reshape(-1, 1, 2).astype(np.float64), np.linalg.inv(H)).reshape(-1, 2)
            tries.append(('through the camera', a, b + [x0, y0]))
    for how, fl, px in tries:
        if len(fl) < 12:
            continue
        Hm, inl = cv2.findHomography(fl, px, cv2.RANSAC, 3.0)
        if inl is None:
            continue
        inl = inl.ravel().astype(bool)
        if inl.sum() > best[0]:
            best = (int(inl.sum()), fl[inl], px[inl].astype(np.float64), how)
    return best


def turn_deg(Ra, Rb):
    return float(np.degrees(np.arccos(np.clip((np.trace(Ra @ Rb.T) - 1) / 2, -1, 1))))


def audit(name, faces, flats):
    from scipy.optimize import least_squares
    img = A.photo(name)
    h, w = img.shape[:2]
    cam = A.reg_camera(name, w, h)
    canvas = img.copy()
    K = np.array([[cam.f, 0, cam.cx], [0, cam.f, cam.cy], [0, 0, 1.0]])
    dist = np.array([cam.k1, 0, 0, 0.0])
    rows, P3, P2, outlines = [], [], [], []
    for fname, face in faces.items():
        n, fl, px, how = find_face(face, flats[fname], img, cam)
        if n < MIN_ON_FACE:
            continue
        X = face.body(fl)
        pred = A.project(cam, X)
        seen = np.isfinite(pred).all(1) & (np.abs(pred) < 20 * max(w, h)).all(1)
        dist_m = np.linalg.norm(X - cam.centre(), axis=1)
        row = {'face': fname, 'found': how, 'on_face': n, 'span_px': round(float(np.ptp(px, axis=0).max()))}
        if seen.mean() < 0.5:
            row['pipeline_camera'] = 'does not see this face: it looks elsewhere'
        else:
            res = (px - pred)[seen]
            d = np.linalg.norm(res, axis=1)
            mm_per_px = 1000.0 * float(np.median(dist_m)) / cam.f
            row.update({'median_px': round(float(np.median(d)), 1), 'p90_px': round(float(np.percentile(d, 90)), 1), 'share_of_width': round(float(np.median(d)) / w, 4),
                        'mean_dx_px': round(float(res[:, 0].mean()), 1), 'mean_dy_px': round(float(res[:, 1].mean()), 1), 'range_m': round(float(np.median(dist_m)), 2), 'median_mm': round(float(np.median(d)) * mm_per_px, 1)})
            step = max(1, int(seen.sum()) // 220)
            for q, r_ in zip(pred[seen][::step], px[seen][::step]):
                cv2.line(canvas, tuple(int(v) for v in q), tuple(int(v) for v in r_), (0, 0, 255), 2, cv2.LINE_AA)
                cv2.circle(canvas, tuple(int(v) for v in r_), 3, (0, 0, 255), -1, cv2.LINE_AA)
        rows.append(row)
        P3.append(X)
        P2.append(px)
        c = face.corners_flat()
        outlines.append(face.body(np.vstack([np.linspace(c[i], c[(i + 1) % 4], 40) for i in range(4)])))
    out = {'photo': name, 'size_px': [w, h], 'f_px': round(cam.f, 1), 'k1': round(cam.k1, 4), 'eye_m': cam.centre().round(3).tolist(), 'looks_along': cam.R[2].round(3).tolist(), 'faces': rows}

    def draw(Xo, rv, tv, Kq, colour):
        p = cv2.projectPoints(Xo, rv, tv, Kq, dist)[0].reshape(-1, 2)
        z = (cv2.Rodrigues(rv)[0] @ Xo.T).T[:, 2] + np.ravel(tv)[2]
        ok = (z > 0.25) & (p[:, 0] > -w) & (p[:, 0] < 2 * w) & (p[:, 1] > -h) & (p[:, 1] < 2 * h)       # a face running past the camera: only what lies in front of it, piece by piece
        for i in range(len(p) - 1):
            if ok[i] and ok[i + 1]:
                cv2.line(canvas, tuple(int(v) for v in p[i]), tuple(int(v) for v in p[i + 1]), colour, 3, cv2.LINE_AA)
    for Xo in outlines:
        draw(Xo, cv2.Rodrigues(cam.R)[0], cam.t.reshape(3, 1), K, (255, 255, 0))
    if P3:
        X, p = np.vstack(P3), np.vstack(P2)
        planar = len(P3) == 1
        ok, rv, tv = cv2.solvePnP(X, p, K, dist, flags=cv2.SOLVEPNP_IPPE if planar else cv2.SOLVEPNP_EPNP)
        ok, rv, tv = cv2.solvePnP(X, p, K, dist, rv, tv, True, cv2.SOLVEPNP_ITERATIVE)
        R = cv2.Rodrigues(rv)[0]
        eye = (-R.T @ tv).ravel()
        e = np.linalg.norm(cv2.projectPoints(X, rv, tv, K, dist)[0].reshape(-1, 2) - p, axis=1)
        out['resection'] = {'what': 'pose from the traced face(s) alone, at the pipeline camera\'s focal length and lens term', 'median_px': round(float(np.median(e)), 1), 'eye_m': eye.round(3).tolist(), 'looks_along': R[2].round(3).tolist(),
                            'pipeline_eye_off_m': round(float(np.linalg.norm(eye - cam.centre())), 3), 'pipeline_turned_deg': round(turn_deg(R, cam.R), 2), 'points': int(len(X)),
                            'camera': A.V.Camera(cam.f, cam.cx, cam.cy, R, tv.ravel(), w, h, cam.k1).to_dict()}

        def fun(q):
            Kq = K.copy()
            Kq[0, 0] = Kq[1, 1] = cam.f * np.exp(q[6])
            return (cv2.projectPoints(X, q[:3], q[3:6], Kq, dist)[0].reshape(-1, 2) - p).ravel()
        s = least_squares(fun, np.concatenate([rv.ravel(), tv.ravel(), [0.0]]), loss='huber', f_scale=2.0)
        R2 = cv2.Rodrigues(s.x[:3])[0]
        eye2 = -R2.T @ s.x[3:6]
        out['resection_f'] = {'what': 'pose and focal length together', 'median_px': round(float(np.median(np.linalg.norm(fun(s.x).reshape(-1, 2), axis=1))), 1), 'f_ratio': round(float(np.exp(s.x[6])), 3), 'eye_m': eye2.round(3).tolist(),
                              'pipeline_eye_off_m': round(float(np.linalg.norm(eye2 - cam.centre())), 3), 'pipeline_turned_deg': round(turn_deg(R2, cam.R), 2)}
        for Xo in outlines:
            draw(Xo, rv, tv, K, (0, 220, 0))
    os.makedirs(OUT, exist_ok=True)
    cv2.imwrite(os.path.join(OUT, name + '.jpg'), canvas, [cv2.IMWRITE_JPEG_QUALITY, 90])
    return out


def main(argv):
    faces = A.faces()
    flats = {k: cv2.imread(f.image) for k, f in faces.items()}
    names = argv or A.registered_views()
    out = {}
    for n in names:
        r = audit(n, faces, flats)
        out[n] = r
        if not r['faces']:
            print('%-7s no traced face found in the photo' % n, flush=True)
        for row in r['faces']:
            if 'median_px' in row:
                print('%-7s %-8s on face %4d (%s)  error %6.1f px median, %6.1f p90 = %5.1f mm at %4.2f m, %.1f %% of the width' % (n, row['face'], row['on_face'], row['found'], row['median_px'], row['p90_px'], row['median_mm'], row['range_m'], 100 * row['share_of_width']), flush=True)
            else:
                print('%-7s %-8s on face %4d (%s)  %s' % (n, row['face'], row['on_face'], row['found'], row['pipeline_camera']), flush=True)
        for k in ('resection', 'resection_f'):
            if k in r:
                print('        %-12s %s' % (k, {a: b for a, b in r[k].items() if a not in ('what', 'camera')}), flush=True)
    p = os.path.join(OUT, 'camera.json')
    prev = json.load(open(p)) if os.path.exists(p) and argv else {}
    prev.update(out)
    with open(p, 'w', encoding='utf-8') as f:
        json.dump(prev, f, indent=1)


if __name__ == '__main__':
    main(sys.argv[1:])
