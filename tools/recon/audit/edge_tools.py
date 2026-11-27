"""Edge chains of a photo, numbered: the edge-line counterpart of SAM 2's automatic masks. The code finds every edge
and measures it to a fraction of a pixel; a person (or Claude) only says which numbered chain is which part's edge.

    python tools/recon/audit/edge_tools.py g098-a 150,900,1200,2123 [--sigma 2.0] [--min 80]

Out: out/recon/audit/edges/<photo>_<box>.jpg (numbered), .json (each chain's points in the photo's pixels, sub-pixel).
"""
import argparse
import json
import os
import sys

import cv2
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as A  # noqa: E402

OUT = os.path.join(A.OUT, 'edges')
N8 = [(-1, -1), (-1, 0), (-1, 1), (0, -1), (0, 1), (1, -1), (1, 0), (1, 1)]


def link(edge):
    """Thin edge pixels as chains: walked from end points, each pixel once."""
    e = edge.copy()
    h, w = e.shape
    nb = cv2.filter2D(e.astype(np.uint8), -1, np.ones((3, 3), np.float32), borderType=cv2.BORDER_CONSTANT) - e.astype(np.uint8)
    chains = []

    def walk(y, x):
        pts = [(x, y)]
        e[y, x] = False
        while True:
            nxt = None
            for dy, dx in N8:
                yy, xx = y + dy, x + dx
                if 0 <= yy < h and 0 <= xx < w and e[yy, xx]:
                    nxt = (yy, xx)
                    if dy == 0 or dx == 0:      # straight neighbours first, so a diagonal step does not skip a pixel
                        break
            if nxt is None:
                return pts
            y, x = nxt
            e[y, x] = False
            pts.append((x, y))                  # Canny's edges are thin but stepped, so a pixel with three neighbours is usually a stair, not a junction: walk on
    ys, xs = np.nonzero(e & (nb == 1))
    for y, x in zip(ys, xs):
        if e[y, x]:
            chains.append(walk(y, x))
    ys, xs = np.nonzero(e)
    for y, x in zip(ys, xs):
        if e[y, x]:
            chains.append(walk(y, x))
    return chains


def chains_of(gray, sigma=2.0, lo=None, hi=None, min_len=80):
    """Edge chains with sub-pixel points: [(N x 2 float array of x, y, mean gradient strength)]."""
    g = cv2.GaussianBlur(gray.astype(np.float32), (0, 0), sigma)
    gx, gy = cv2.Sobel(g, cv2.CV_32F, 1, 0, ksize=3), cv2.Sobel(g, cv2.CV_32F, 0, 1, ksize=3)
    mag = np.hypot(gx, gy)
    hi = hi or float(np.percentile(mag, 80))
    lo = lo or 0.35 * hi
    q = 16.0                                # Canny takes whole numbers: keep the weak gradients of a blurred print
    e = cv2.Canny(np.clip(gx * q, -32000, 32000).astype(np.int16), np.clip(gy * q, -32000, 32000).astype(np.int16), lo * q, hi * q, L2gradient=True) > 0
    out = []
    for c in link(e):
        if len(c) < min_len:
            continue
        p = np.array(c, float)
        xi, yi = p[:, 0].astype(int), p[:, 1].astype(int)
        m0 = mag[yi, xi]
        nx, ny = gx[yi, xi] / np.maximum(m0, 1e-6), gy[yi, xi] / np.maximum(m0, 1e-6)
        mp = cv2.remap(mag, (p[:, 0] + nx).astype(np.float32), (p[:, 1] + ny).astype(np.float32), cv2.INTER_LINEAR).ravel()
        mm = cv2.remap(mag, (p[:, 0] - nx).astype(np.float32), (p[:, 1] - ny).astype(np.float32), cv2.INTER_LINEAR).ravel()
        den = mp - 2 * m0 + mm
        off = np.where(np.abs(den) > 1e-6, 0.5 * (mm - mp) / den, 0.0).clip(-1, 1)      # the gradient's peak across the edge, by a parabola
        p[:, 0] += off * nx
        p[:, 1] += off * ny
        k = max(3, int(2 * sigma) | 1)
        ps = np.stack([np.convolve(np.pad(p[:, i], k // 2, mode='edge'), np.ones(k) / k, mode='valid') for i in (0, 1)], 1)
        out.append((ps, float(m0.mean())))
    out.sort(key=lambda q: -len(q[0]))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('photo')
    ap.add_argument('box')
    ap.add_argument('--sigma', type=float, default=2.0)
    ap.add_argument('--min', type=int, default=80)
    ap.add_argument('--scale', type=float, default=1.0)
    a = ap.parse_args()
    img = A.photo(a.photo)
    x0, y0, x1, y1 = [int(v) for v in a.box.split(',')]
    sub = img[y0:y1, x0:x1]
    ch = chains_of(cv2.cvtColor(sub, cv2.COLOR_BGR2GRAY), a.sigma, min_len=a.min)
    canvas = cv2.addWeighted(sub, 0.75, np.full_like(sub, 255), 0.25, 0)
    if a.scale != 1.0:
        canvas = cv2.resize(canvas, None, fx=a.scale, fy=a.scale, interpolation=cv2.INTER_CUBIC)
    rng = np.random.default_rng(1)
    for i, (p, s) in enumerate(ch):
        col = [int(v) for v in rng.integers(0, 200, 3)]
        cv2.polylines(canvas, [np.round(p * a.scale).astype(np.int32)], False, col, 2, cv2.LINE_AA)
    for i, (p, s) in enumerate(ch):
        q = tuple(int(v) for v in p[len(p) // 2] * a.scale)
        cv2.putText(canvas, str(i), q, 0, 0.5, (255, 255, 255), 3, cv2.LINE_AA)
        cv2.putText(canvas, str(i), q, 0, 0.5, (0, 0, 0), 1, cv2.LINE_AA)
    os.makedirs(OUT, exist_ok=True)
    stem = os.path.join(OUT, '%s_%s' % (a.photo, a.box.replace(',', '_')))
    cv2.imwrite(stem + '.jpg', canvas, [cv2.IMWRITE_JPEG_QUALITY, 92])
    with open(stem + '.json', 'w', encoding='utf-8') as f:
        json.dump({'photo': a.photo, 'box': [x0, y0, x1, y1], 'sigma': a.sigma, 'chains': [{'n': i, 'strength': round(s, 1), 'xy': (p + [x0, y0]).round(2).tolist()} for i, (p, s) in enumerate(ch)]}, f)
    print(len(ch), 'chains ->', stem + '.jpg', canvas.shape)


if __name__ == '__main__':
    main()
