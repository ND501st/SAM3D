"""Identification audit (handoff section 8.4): the boxes the first pass drew by eye (tools/traces/osa_interior/observations)
against SAM 2's masks of the same parts, and against where the model has the part through the pipeline's camera.

    python tools/recon/audit/ident_audit.py

Out: out/recon/audit/ident.json
"""
import json
import os
import sys

import cv2
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as A  # noqa: E402
import part_audit as PA  # noqa: E402
from common import DF, V  # noqa: E402

OBS = os.path.join(A.REPO, 'tools', 'traces', 'osa_interior', 'observations')
# photo part (a SAM 2 mask) -> the observation's id in that photo
SAME = {'g098-a': {'sp3m_flap': 'new.cover.sp3m_front_flap', 'first_aid': 'kit.first_aid', 'chest_5937': 'new.box.stowage_5937', 'lamp_blue': 'lamp.blue', 'housing': 'new.seat.rear_right_folding'},
        'g096-a': {'cabinet_0096': 'rack.0096_9m', 'first_aid': 'kit.first_aid', 'chest_5937': 'new.box.stowage_5937', 'lamp_blue': 'lamp.blue'}}


def iou(a, b):
    x0, y0, x1, y1 = max(a[0], b[0]), max(a[1], b[1]), min(a[2], b[2]), min(a[3], b[3])
    i = max(0, x1 - x0) * max(0, y1 - y0)
    return i / ((a[2] - a[0]) * (a[3] - a[1]) + (b[2] - b[0]) * (b[3] - b[1]) - i)


def box(m):
    ys, xs = np.nonzero(m)
    return [int(xs.min()), int(ys.min()), int(xs.max()) + 1, int(ys.max()) + 1] if len(xs) else None


def main():
    out = {}
    for name, same in SAME.items():
        img = A.photo(name)
        h, w = img.shape[:2]
        obs = {c['id']: c['bbox'] for c in json.load(open(os.path.join(OBS, name + '.json'), encoding='utf-8'))['components']}
        view = V.load(V.view_path(name)) if os.path.exists(V.view_path(name)) else {}
        scene = DF.Scene(tuple(sorted(view.get('state', {}).get('hide', []))))
        mm = PA.model_masks(A.reg_camera(name, w, h), scene, {p: PA.PARTS[name][p] for p in same})
        for part, oid in same.items():
            sam = box(cv2.imread(os.path.join(PA.MASKS, name, part + '.png'), 0) > 0)
            eye = [obs[oid][0] * w, obs[oid][1] * h, obs[oid][2] * w, obs[oid][3] * h]
            mod = box(mm[part])
            edge = max(abs(a - b) for a, b in zip(eye, sam))
            out['%s %s' % (name, part)] = {'eye_box_iou': round(iou(eye, sam), 2), 'eye_box_worst_edge_px': round(edge), 'eye_box_worst_edge_share': round(edge / max(w, h), 3), 'model_box_iou': round(iou(mod, sam), 2) if mod else None}
            print('%-7s %-13s eye box against the mask: IoU %.2f, worst edge %4d px (%.1f %% of the frame)   the model against the mask: IoU %s' % (name, part, iou(eye, sam), edge, 100 * edge / max(w, h), ('%.2f' % iou(mod, sam)) if mod else '-'))
    with open(os.path.join(A.OUT, 'ident.json'), 'w', encoding='utf-8') as f:
        json.dump(out, f, indent=1)


if __name__ == '__main__':
    main()
