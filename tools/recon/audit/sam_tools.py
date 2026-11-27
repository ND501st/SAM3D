"""SAM 2.1 for the audit: masks by box or point prompts, and every mask of a photo with a numbered overlay.

    python tools/recon/audit/sam_tools.py auto g098-a [--side 32] [--crop x0,y0,x1,y1 as shares]

Out: out/recon/audit/masks/<photo>[_crop]/auto.npz (masks, boxes, scores), auto.jpg (numbered).
"""
import argparse
import os
import sys

import cv2
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as A  # noqa: E402

CKPT = r'E:\SAM3D-tools\models\sam2.1_hiera_base_plus.pt'
CFG = 'configs/sam2.1/sam2.1_hiera_b+.yaml'
OUT = os.path.join(A.OUT, 'masks')
_M = {}


def model():
    if 'm' not in _M:
        from sam2.build_sam import build_sam2
        _M['m'] = build_sam2(CFG, CKPT, device='cuda')
    return _M['m']


class Prompter:
    """One photo set once; masks by box and by points, in the photo's own pixels."""

    def __init__(self, bgr):
        import torch
        from sam2.sam2_image_predictor import SAM2ImagePredictor
        self.t = torch
        self.p = SAM2ImagePredictor(model())
        with torch.inference_mode(), torch.autocast('cuda', dtype=torch.bfloat16):
            self.p.set_image(cv2.cvtColor(bgr, cv2.COLOR_BGR2RGB))

    def ask(self, box=None, points=None, labels=None, multi=True):
        with self.t.inference_mode(), self.t.autocast('cuda', dtype=self.t.bfloat16):
            m, s, _ = self.p.predict(box=None if box is None else np.asarray(box, float)[None, :], point_coords=None if points is None else np.asarray(points, float),
                                     point_labels=None if points is None else np.asarray(labels if labels is not None else [1] * len(points)), multimask_output=multi)
        return m.astype(bool), s


def bbox(mask):
    ys, xs = np.nonzero(mask)
    return [int(xs.min()), int(ys.min()), int(xs.max()) + 1, int(ys.max()) + 1] if len(xs) else None


def auto(name, side, crop):
    import torch
    from sam2.automatic_mask_generator import SAM2AutomaticMaskGenerator
    img = A.photo(name)
    h, w = img.shape[:2]
    x0, y0, x1, y1 = (int(crop[0] * w), int(crop[1] * h), int(crop[2] * w), int(crop[3] * h)) if crop else (0, 0, w, h)
    sub = img[y0:y1, x0:x1]
    gen = SAM2AutomaticMaskGenerator(model(), points_per_side=side, points_per_batch=32, pred_iou_thresh=0.7, stability_score_thresh=0.9, box_nms_thresh=0.7, min_mask_region_area=400)
    with torch.inference_mode(), torch.autocast('cuda', dtype=torch.bfloat16):
        res = gen.generate(cv2.cvtColor(sub, cv2.COLOR_BGR2RGB))
    res.sort(key=lambda r: -r['area'])
    d = os.path.join(OUT, name + ('_crop' if crop else ''))
    os.makedirs(d, exist_ok=True)
    masks = np.stack([r['segmentation'] for r in res]) if res else np.zeros((0,) + sub.shape[:2], bool)
    np.savez_compressed(os.path.join(d, 'auto.npz'), masks=np.packbits(masks, axis=-1), shape=masks.shape, offset=[x0, y0], scores=[r['predicted_iou'] for r in res])
    canvas = sub.copy()
    rng = np.random.default_rng(3)
    for i, m in enumerate(masks):
        col = rng.integers(60, 255, 3).tolist()
        cnt, _ = cv2.findContours(m.astype(np.uint8), cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)
        cv2.drawContours(canvas, cnt, -1, col, 2)
    for i, m in enumerate(masks):
        ys, xs = np.nonzero(m)
        c = (int(np.median(xs)), int(np.median(ys)))
        if not m[c[1], c[0]]:
            j = np.argmin((xs - c[0]) ** 2 + (ys - c[1]) ** 2)
            c = (int(xs[j]), int(ys[j]))
        cv2.putText(canvas, str(i), c, 0, 0.7, (0, 0, 0), 4, cv2.LINE_AA)
        cv2.putText(canvas, str(i), c, 0, 0.7, (255, 255, 255), 1, cv2.LINE_AA)
    cv2.imwrite(os.path.join(d, 'auto.jpg'), canvas, [cv2.IMWRITE_JPEG_QUALITY, 90])
    print(name, len(masks), 'masks ->', d)


def ask(name, parts, view=None):
    """Named parts by point prompts: parts = {part: {'pos': [[x, y], ...], 'neg': [...], 'box': [x0, y0, x1, y1]}}, in the clean photo's pixels.
    Each part's mask is kept as masks/<photo>/<part>.png; one overlay shows them all. The points only say which thing is meant: the outline is SAM's."""
    import json
    img = A.photo(name)
    d = os.path.join(OUT, name)
    os.makedirs(d, exist_ok=True)
    pr = Prompter(img)
    canvas = img.copy()
    rng = np.random.default_rng(5)
    out = {}
    for part, q in parts.items():
        pts = [list(p) for p in q.get('pos', [])] + [list(p) for p in q.get('neg', [])]
        lab = [1] * len(q.get('pos', [])) + [0] * len(q.get('neg', []))
        m, s = pr.ask(box=q.get('box'), points=pts or None, labels=lab or None, multi=len(pts) <= 1 and 'box' not in q)
        i = int(q['pick']) if 'pick' in q else int(np.argmax(s))
        mask = m[i]
        if q.get('largest', True):                              # the piece holding the first positive point, or the largest
            n, lab_im = cv2.connectedComponents(mask.astype(np.uint8))
            if n > 2:
                first = q.get('pos', [[None]])[0]
                keep = lab_im[int(first[1]), int(first[0])] if first[0] is not None and lab_im[int(first[1]), int(first[0])] > 0 else 1 + int(np.argmax(np.bincount(lab_im[lab_im > 0])[1:]))
                mask = lab_im == keep
        cv2.imwrite(os.path.join(d, part + '.png'), mask.astype(np.uint8) * 255)
        col = rng.integers(80, 255, 3).tolist()
        cnt, _ = cv2.findContours(mask.astype(np.uint8), cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)
        cv2.drawContours(canvas, cnt, -1, col, 2)
        for p, l_ in zip(pts, lab):
            cv2.drawMarker(canvas, (int(p[0]), int(p[1])), (0, 255, 0) if l_ else (0, 0, 255), cv2.MARKER_CROSS, 14, 2)
        b = bbox(mask)
        cv2.putText(canvas, part, (b[0], max(14, b[1] - 4)), 0, 0.55, (0, 0, 0), 4, cv2.LINE_AA)
        cv2.putText(canvas, part, (b[0], max(14, b[1] - 4)), 0, 0.55, col, 1, cv2.LINE_AA)
        out[part] = {'score': round(float(s[i]), 3), 'area_px': int(mask.sum()), 'bbox': b}
        print('%-28s score %.2f  area %7d  box %s' % (part, s[i], mask.sum(), b))
    if view:
        x0, y0, x1, y1 = view
        canvas = canvas[y0:y1, x0:x1]
    cv2.imwrite(os.path.join(d, 'parts.jpg'), canvas, [cv2.IMWRITE_JPEG_QUALITY, 90])
    with open(os.path.join(d, 'parts.json'), 'w', encoding='utf-8') as f:
        json.dump({'prompts': parts, 'masks': out}, f, indent=1)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('stage', choices=['auto', 'ask'])
    ap.add_argument('photos', nargs='+')
    ap.add_argument('--side', type=int, default=32)
    ap.add_argument('--crop')
    ap.add_argument('--parts', help='ask: a JSON file of prompts')
    ap.add_argument('--view', help='ask: x0,y0,x1,y1 of the overlay to write')
    a = ap.parse_args()
    if a.stage == 'ask':
        import json
        ask(a.photos[0], json.load(open(a.parts, encoding='utf-8')), [int(v) for v in a.view.split(',')] if a.view else None)
        return
    crop = [float(v) for v in a.crop.split(',')] if a.crop else None
    for p in a.photos:
        auto(p, a.side, crop)


if __name__ == '__main__':
    main()
