"""Part outlines (handoff section 8.2): each named part's outline in the photo (a SAM 2 mask, sam_tools.py ask) against
the model's same part seen through the view's camera, in pixels of the descreened photo.

Two cameras: the one every judgement was made through (holistic_diff's), and the one a resection on the traced face
gives (camera_audit.py). The difference between the two columns is what the camera costs; what is left under the
resected camera is the part itself: its place, its size, its shape.

    python tools/recon/audit/part_audit.py g098-a

Out: out/recon/audit/parts/<photo>.jpg (photo outline green, model through the pipeline's camera red, through the
resected camera yellow), <photo>.json.
"""
import json
import os
import sys

import cv2
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as A  # noqa: E402
from common import DF, V  # noqa: E402

OUT = os.path.join(A.OUT, 'parts')
MASKS = os.path.join(A.OUT, 'masks')

# photo part -> the model's part: a cabin part's id, or a box of the hull (x, y, z ranges, metres) for what the hull mesh holds
PARTS = {
    'g098-a': {
        'sp3m_flap': 'sp3m_flap',
        'first_aid': 'first_aid',
        'chest_5937': 'chest_5937',
        'lamp_blue': 'lamp_blue',
        'housing': ((0.36, 0.80), (0.265, 0.405), (0.62, 1.45)),       # posts.right.shroud: the "cover" of section 41
    },
    'g096-a': {'cabinet_0096': 'cabinet_0096', 'first_aid': 'first_aid', 'chest_5937': 'chest_5937', 'lamp_blue': 'lamp_blue', 'sp3m_flap': 'sp3m_flap'},
}


def model_masks(cam, scene, spec):
    h, w = cam.h, cam.w
    y, x = np.mgrid[0:h, 0:w]
    ray = DF.rays_of(cam, x.ravel() + 0.5, y.ravel() + 0.5)
    d = (cam.R.T @ (ray / np.linalg.norm(ray, axis=1, keepdims=True)).T).T
    tri, _ = scene.cast(cam.centre(), d)
    tri = tri.reshape(h, w)
    hit = tri >= 0
    pid = {n: i for i, n in enumerate(scene.names)}
    out = {}
    for name, m in spec.items():
        if isinstance(m, str):
            sel = scene.part == pid[m]
        else:
            lo, hi = np.array([b[0] for b in m]), np.array([b[1] for b in m])
            sel = (scene.part == pid['hull']) & np.all((scene.tri >= lo) & (scene.tri <= hi), axis=(1, 2))
        mask = np.zeros((h, w), bool)
        mask[hit] = sel[tri[hit]]
        out[name] = mask
    return out


def contour(mask):
    c, _ = cv2.findContours(mask.astype(np.uint8), cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)
    return np.vstack([q.reshape(-1, 2) for q in c]) if c else np.zeros((0, 2), int)


def compare(a, b):
    """Photo mask a against model mask b: overlap, and how far each outline is from the other."""
    if not b.any():
        return {'model': 'not in view'}
    inter, union = (a & b).sum(), (a | b).sum()
    da = cv2.distanceTransform((~_edge(a)).astype(np.uint8), cv2.DIST_L2, 5)
    db = cv2.distanceTransform((~_edge(b)).astype(np.uint8), cv2.DIST_L2, 5)
    ca, cb = contour(a), contour(b)
    d = np.concatenate([db[ca[:, 1], ca[:, 0]], da[cb[:, 1], cb[:, 0]]])
    ma, mb = np.argwhere(a).mean(0)[::-1], np.argwhere(b).mean(0)[::-1]
    return {'iou': round(float(inter / union), 3), 'outline_mean_px': round(float(d.mean()), 1), 'outline_p90_px': round(float(np.percentile(d, 90)), 1), 'outline_max_px': round(float(d.max()), 1),
            'centre_off_px': [round(float(mb[0] - ma[0]), 1), round(float(mb[1] - ma[1]), 1)], 'area_ratio': round(float(b.sum() / a.sum()), 2)}


def _edge(m):
    e = np.zeros(m.shape, bool)
    c = contour(m)
    e[c[:, 1], c[:, 0]] = True
    return e


def main(argv):
    cams = json.load(open(os.path.join(A.OUT, 'camera', 'camera.json')))
    os.makedirs(OUT, exist_ok=True)
    for name in argv:
        img = A.photo(name)
        h, w = img.shape[:2]
        view = V.load(V.view_path(name)) if os.path.exists(V.view_path(name)) else {}
        scene = DF.Scene(tuple(sorted(view.get('state', {}).get('hide', []))))
        pipe = A.reg_camera(name, w, h)
        res = V.Camera.from_dict(cams[name]['resection']['camera']) if 'resection' in cams.get(name, {}) else None
        spec = PARTS[name]
        photo_masks = {p: cv2.imread(os.path.join(MASKS, name, p + '.png'), 0) > 0 for p in spec}
        mm_per_px = {}
        rows = {}
        canvas = img.copy()
        for label, cam, colour in (('pipeline camera', pipe, (0, 0, 255)), ('resected camera', res, (0, 230, 255))):
            if cam is None:
                continue
            mm = model_masks(cam, scene, spec)
            for p in spec:
                r = compare(photo_masks[p], mm[p])
                rows.setdefault(p, {})[label] = r
                cv2.drawContours(canvas, cv2.findContours(mm[p].astype(np.uint8), cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)[0], -1, colour, 2)
        for p in spec:
            cv2.drawContours(canvas, cv2.findContours(photo_masks[p].astype(np.uint8), cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)[0], -1, (0, 255, 0), 2)
            ys, xs = np.nonzero(photo_masks[p])
            cv2.putText(canvas, p, (int(xs.min()), int(ys.min()) - 6), 0, 0.6, (0, 0, 0), 4, cv2.LINE_AA)
            cv2.putText(canvas, p, (int(xs.min()), int(ys.min()) - 6), 0, 0.6, (0, 255, 0), 1, cv2.LINE_AA)
        cv2.putText(canvas, 'green: the photo (SAM 2)   red: the model through the pipeline\'s camera   yellow: through the camera resected on the traced face', (10, h - 12), 0, 0.5, (0, 0, 0), 4, cv2.LINE_AA)
        cv2.putText(canvas, 'green: the photo (SAM 2)   red: the model through the pipeline\'s camera   yellow: through the camera resected on the traced face', (10, h - 12), 0, 0.5, (255, 255, 255), 1, cv2.LINE_AA)
        cv2.imwrite(os.path.join(OUT, name + '.jpg'), canvas, [cv2.IMWRITE_JPEG_QUALITY, 92])
        with open(os.path.join(OUT, name + '.json'), 'w', encoding='utf-8') as f:
            json.dump(rows, f, indent=1)
        for p, r in rows.items():
            for label, q in r.items():
                print('%-7s %-12s %-16s %s' % (name, p, label, q))


if __name__ == '__main__':
    main(sys.argv[1:])
