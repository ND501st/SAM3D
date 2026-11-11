"""Representation audit (handoff section 8.5), the part that can be measured without knowing the true 3-D shape:
how many outline vertices each photographed part needs to be followed to a few pixels, against how many numbers the
model gives it.

    python tools/recon/audit/repr_audit.py

Out: out/recon/audit/repr.json
"""
import json
import os
import sys

import cv2
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as A  # noqa: E402

MASKS = os.path.join(A.OUT, 'masks')
PARTS = [('g098-a', 'housing', 13, 'posts.right.shroud.outline_x_z_m, one section carried 0.12 m along y'), ('g098-a', 'first_aid', 4, 'a rounded box'), ('g098-a', 'chest_5937', 4, 'a box in layers'),
         ('g098-a', 'sp3m_flap', 4, 'a plate'), ('g096-a', 'cabinet_0096', None, 'traced in mm')]


def simplify(c, eps):
    return cv2.approxPolyDP(c, eps, True)


def deviation(c, poly):
    m = np.zeros((c[:, 0, 1].max() + 3, c[:, 0, 0].max() + 3), np.uint8)
    cv2.polylines(m, [poly], True, 255, 1)
    d = cv2.distanceTransform(255 - m, cv2.DIST_L2, 5)
    return float(d[c[:, 0, 1], c[:, 0, 0]].max())


def main():
    out = {}
    for photo, part, given, what in PARTS:
        m = cv2.imread(os.path.join(MASKS, photo, part + '.png'), 0)
        m = cv2.morphologyEx(m, cv2.MORPH_OPEN, np.ones((5, 5), np.uint8))            # the mask's own ragged pixels are not shape
        cs, _ = cv2.findContours(m, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)
        c = max(cs, key=cv2.contourArea)
        row = {'outline_px': int(len(c))}
        for tol in (2, 3, 5, 10):
            lo, hi = 0.2, 80.0
            for _ in range(30):
                mid = 0.5 * (lo + hi)
                if deviation(c, simplify(c, mid)) <= tol:
                    lo = mid
                else:
                    hi = mid
            row['vertices_for_%dpx' % tol] = int(len(simplify(c, lo)))
        if given:
            lo, hi = 0.2, 300.0
            for _ in range(40):
                mid = 0.5 * (lo + hi)
                if len(simplify(c, mid)) > given:
                    lo = mid
                else:
                    hi = mid
            row['given_vertices'] = given
            row['best_error_with_given_px'] = round(deviation(c, simplify(c, hi)), 1)
        row['model_gives'] = what
        out['%s %s' % (photo, part)] = row
        print(photo, part, row)
    with open(os.path.join(A.OUT, 'repr.json'), 'w', encoding='utf-8') as f:
        json.dump(out, f, indent=1)


if __name__ == '__main__':
    main()
