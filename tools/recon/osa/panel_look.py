"""A part of the assembled interior beside the photo, from panel_score.py's renders: the photo, the photo with the model's
edges (yellow) and the part's own outline (magenta), cropped round the part with a margin, for looking at.

    python tools/recon/osa/panel_look.py VIEW PART [--pad 120] [--scale 1.0]

Writes out/recon/osa/panel_score/look_<view>_<part>.jpg.
"""
import argparse
import json
import os
import sys

import cv2
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import panel_score as PS  # noqa: E402
V = PS.V


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('view')
    ap.add_argument('part')
    ap.add_argument('--pad', type=int, default=120)
    ap.add_argument('--scale', type=float, default=1.0)
    a = ap.parse_args(argv)
    width = V.load(os.path.join(PS.OUT, 'score.json'))['width']
    cam = PS.certified(a.view, width)
    prefix = os.path.join(PS.OUT, a.view + '_')
    photo = cv2.resize(cv2.imread(V.photo_file(a.view, 'clean')), (cam.w, cam.h), interpolation=cv2.INTER_AREA)
    em = PS.edges(cv2.imread(prefix + 'model.png'))
    ids = cv2.cvtColor(cv2.imread(prefix + 'id.png'), cv2.COLOR_BGR2RGB)
    rgb = json.load(open(prefix + 'id.json', encoding='utf-8'))['by_part'][a.part]
    mask = np.all(ids == np.array(rgb, np.uint8), axis=-1).astype(np.uint8)
    ys, xs = np.nonzero(mask)
    x0, y0 = max(0, xs.min() - a.pad), max(0, ys.min() - a.pad)
    x1, y1 = min(cam.w, xs.max() + a.pad), min(cam.h, ys.max() + a.pad)
    over = photo.copy()
    over[em > 0] = (0, 255, 255)
    cnt, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)
    cv2.drawContours(over, cnt, -1, (255, 0, 255), 2)
    out = np.hstack([photo[y0:y1, x0:x1], np.zeros((y1 - y0, 8, 3), np.uint8), over[y0:y1, x0:x1]])
    if a.scale != 1.0:
        out = cv2.resize(out, None, fx=a.scale, fy=a.scale, interpolation=cv2.INTER_AREA)
    path = os.path.join(PS.OUT, 'look_%s_%s.jpg' % (a.view, a.part))
    cv2.imwrite(path, out, [cv2.IMWRITE_JPEG_QUALITY, 90])
    print(path, out.shape)
    return 0


if __name__ == '__main__':
    sys.exit(main())
