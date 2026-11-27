"""A crop of a photo (or any image) with a labelled grid in the full image's own pixels, for looking at a part and
choosing prompt points. Numbers are never read off these by eye for measurement: prompts only.

    python tools/recon/audit/look.py g098-a 200,800,1100,2000 [--step 100] [--scale 1.0] [--image path] [--out path]
"""
import argparse
import os
import sys

import cv2

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as A  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('photo')
    ap.add_argument('box')
    ap.add_argument('--step', type=int, default=100)
    ap.add_argument('--scale', type=float, default=1.0)
    ap.add_argument('--image')
    ap.add_argument('--out')
    ap.add_argument('--kind', default='clean')
    a = ap.parse_args()
    img = cv2.imread(a.image) if a.image else A.photo(a.photo, a.kind)
    x0, y0, x1, y1 = [int(v) for v in a.box.split(',')]
    sub = img[y0:y1, x0:x1].copy()
    if a.scale != 1.0:
        sub = cv2.resize(sub, None, fx=a.scale, fy=a.scale, interpolation=cv2.INTER_AREA if a.scale < 1 else cv2.INTER_CUBIC)
    for x in range((x0 // a.step + 1) * a.step, x1, a.step):
        px = int((x - x0) * a.scale)
        cv2.line(sub, (px, 0), (px, sub.shape[0]), (255, 255, 0), 1)
        cv2.putText(sub, str(x), (px + 2, 12), 0, 0.4, (0, 0, 0), 3, cv2.LINE_AA)
        cv2.putText(sub, str(x), (px + 2, 12), 0, 0.4, (255, 255, 0), 1, cv2.LINE_AA)
    for y in range((y0 // a.step + 1) * a.step, y1, a.step):
        py = int((y - y0) * a.scale)
        cv2.line(sub, (0, py), (sub.shape[1], py), (255, 255, 0), 1)
        cv2.putText(sub, str(y), (2, py - 3), 0, 0.4, (0, 0, 0), 3, cv2.LINE_AA)
        cv2.putText(sub, str(y), (2, py - 3), 0, 0.4, (255, 255, 0), 1, cv2.LINE_AA)
    out = a.out or os.path.join(A.OUT, 'look', '%s_%s.jpg' % (a.photo, a.box.replace(',', '_')))
    os.makedirs(os.path.dirname(out), exist_ok=True)
    cv2.imwrite(out, sub, [cv2.IMWRITE_JPEG_QUALITY, 92])
    print(out, sub.shape)


if __name__ == '__main__':
    main()
