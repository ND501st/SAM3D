"""Flatten the R-123 museum photograph the R-123M's front is traced from into a true-scale front view (5 px/mm), the frame
tools/r123m_sketch.py draws the trace in (tools/blender/r123m_trace.py says how the trace was made).

    python tools/r123m_rectify.py path/to/Funkstation_R-123_IMG_1390.JPG      # writes out/r123m/front_mm.png
    python tools/r123m_rectify.py photo.jpg --check                           # also out/r123m/rectify_lines.jpg

The photo (Wikimedia Commons, `r123_commons`, 3000 x 2250 px) is not in the repository. Needs numpy and OpenCV (cv2).

1. Each of the twelve straight edges below, which lie in the plate, is found in its window of the photo (the strongest
   gradient across the window, column by column or row by row) and fitted as a line with RANSAC.
2. The horizontal ones meet in one vanishing point, the vertical ones in another (least squares); the homography that
   sends both to infinity, along x and along y, flattens the plate (the residuals are under 0.65 degrees).
3. The flattened plate's top, left and bottom edges (PLATE_EDGES, in the flattened frame) give the scale: 225 mm high.
"""
import argparse
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
K, MARGIN = 5.0, 40.0                     # px per mm and the margin in mm, as tools/r123m_sketch.py
W_MM, H_MM = 406.0, 225.0

# (axis, a0, a1, b0, b1): 'h' a horizontal edge searched for in rows b0..b1 over columns a0..a1; 'v' a vertical edge in
# columns b0..b1 over rows a0..a1. Photo pixels.
EDGES = {
    'h_top': ('h', 200, 2800, 370, 450),          # the plate's top edge
    'h_fixtop': ('h', 2380, 2860, 440, 495),      # the fixator panel's top
    'h_volt': ('h', 140, 500, 685, 740),          # the voltage plate's top
    'h_bottom': ('h', 2000, 2800, 1735, 1815),    # the plate's bottom edge, right of the drum
    'h_calib': ('h', 610, 870, 1355, 1405),       # the КАЛИБРОВКА plate's bottom
    'v_fixL': ('v', 490, 860, 2320, 2390),        # the fixator panel's left edge
    'v_fixR': ('v', 500, 860, 2870, 2920),        # and its right edge
    'v_korr': ('v', 440, 680, 880, 930),          # the КОРРЕКТОР strip
    'v_volt': ('v', 735, 985, 512, 565),          # the voltage plate's right edge
    'v_calib': ('v', 1175, 1365, 878, 928),       # the КАЛИБРОВКА plate's right edge
    'v_sub': ('v', 670, 870, 2232, 2285),         # the ПОДДИАПАЗ strip by the fixators
}
# The plate's edges in the flattened frame (step 2's output moved by OFFSET): top y, left x, bottom y, px.
OFFSET = (500.0, 100.0)
PLATE_EDGES = {'top': 503.0, 'left': 428.0, 'bottom': 2095.0}


def fit_line(gx, gy, spec, np):
    axis, a0, a1, b0, b1 = spec
    pts = []
    for a in range(a0, a1, 4):
        if axis == 'h':
            col = np.abs(gy[b0:b1, a])
            pts.append((a, b0 + int(np.argmax(col))))
        else:
            row = np.abs(gx[a, b0:b1])
            pts.append((b0 + int(np.argmax(row)), a))
    P = np.array(pts, float)
    rng = np.random.RandomState(0)
    best = None
    for _ in range(400):
        i, j = rng.choice(len(P), 2, replace=False)
        d = P[j] - P[i]
        n = np.array([-d[1], d[0]])
        if np.linalg.norm(n) < 1e-9:
            continue
        n /= np.linalg.norm(n)
        inl = np.abs((P - P[i]) @ n) < 2.5
        if best is None or inl.sum() > best.sum():
            best = inl
    Q = P[best]
    c = Q.mean(0)
    _, _, vt = np.linalg.svd(Q - c)
    return c, vt[0]                    # a point and the direction


def homogeneous(line, np):
    (x0, y0), (vx, vy) = line
    h = np.cross([x0, y0, 1.0], [x0 + vx, y0 + vy, 1.0])
    return h / np.linalg.norm(h[:2])


def vanishing_point(lines, np):
    _, _, vt = np.linalg.svd(np.array([homogeneous(l, np) for l in lines]))
    return vt[-1]


def rectifying_homography(lines, np):
    vh = vanishing_point([l for k, l in lines.items() if k.startswith('h_')], np)
    vv = vanishing_point([l for k, l in lines.items() if k.startswith('v_')], np)
    linf = np.cross(vh, vv)
    linf = linf / linf[2]
    P = np.array([[1, 0, 0], [0, 1, 0], linf])
    dh, dv = (P @ vh)[:2], (P @ vv)[:2]
    dh, dv = dh / np.linalg.norm(dh), dv / np.linalg.norm(dv)
    if dh[0] < 0:
        dh = -dh
    if dv[1] < 0:
        dv = -dv
    M = np.eye(3)
    M[:2, :2] = np.linalg.inv(np.array([[dh[0], dv[0]], [dh[1], dv[1]]]))
    T = np.array([[1, 0, OFFSET[0]], [0, 1, OFFSET[1]], [0, 0, 1.0]])
    s = H_MM / (PLATE_EDGES['bottom'] - PLATE_EDGES['top'])
    S = np.array([[s * K, 0, (-PLATE_EDGES['left'] * s + MARGIN) * K], [0, s * K, (-PLATE_EDGES['top'] * s + MARGIN) * K],
                  [0, 0, 1.0]])
    return S @ T @ M @ P, vh, vv


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('photo')
    ap.add_argument('--out', default=os.path.join(REPO, 'out', 'r123m', 'front_mm.png'))
    ap.add_argument('--check', action='store_true', help='also draw the fitted lines on the photo')
    args = ap.parse_args()
    try:
        import cv2
        import numpy as np
    except ImportError:
        sys.exit('needs numpy and opencv-python')
    im = cv2.imread(args.photo)
    if im is None or im.shape[:2] != (2250, 3000):
        sys.exit('expected the 3000 x 2250 Commons photo')
    g = cv2.GaussianBlur(cv2.cvtColor(im, cv2.COLOR_BGR2GRAY).astype(np.float32), (7, 7), 0)
    gx = cv2.Sobel(g, cv2.CV_32F, 1, 0, ksize=5)
    gy = cv2.Sobel(g, cv2.CV_32F, 0, 1, ksize=5)
    lines = {k: fit_line(gx, gy, spec, np) for k, spec in EDGES.items()}
    H, vh, vv = rectifying_homography(lines, np)
    print('vanishing points', (vh[:2] / vh[2]).round(0), (vv[:2] / vv[2]).round(0))
    for k, l in lines.items():
        v = vh if k.startswith('h_') else vv
        d = v[:2] / v[2] - l[0]
        d /= np.linalg.norm(d)
        print('  %-9s off its vanishing point by %.2f deg' % (k, np.degrees(np.arcsin(abs(d[0] * l[1][1] - d[1] * l[1][0])))))
    size = (int((W_MM + 2 * MARGIN) * K), int((H_MM + 2 * MARGIN) * K))
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    cv2.imwrite(args.out, cv2.warpPerspective(im, H, size, flags=cv2.INTER_CUBIC))
    print('wrote', args.out)
    if args.check:
        for (x0, y0), (vx, vy) in lines.values():
            cv2.line(im, (int(x0 - vx * 4000), int(y0 - vy * 4000)), (int(x0 + vx * 4000), int(y0 + vy * 4000)), (0, 0, 255), 2)
        out = os.path.join(os.path.dirname(os.path.abspath(args.out)), 'rectify_lines.jpg')
        cv2.imwrite(out, cv2.resize(im, None, fx=0.5, fy=0.5))
        print('wrote', out)


if __name__ == '__main__':
    main()
