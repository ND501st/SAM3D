"""Algorithms of the interior pipeline (docs/OSA_INTERIOR_PIPELINE.md): a cabin is a box of flat faces, so every photo
of it carries its own calibration in its straight edges.

* `line_segments`: straight segments (OpenCV's LSD) on a descreened photo.
* `vanishing_points`: up to three dominant vanishing points by RANSAC over the segments, each refined on its inliers.
* `intrinsics_from_vps`: the focal length and the principal point from orthogonal vanishing points (the cabin's
  three axes): with three finite points the principal point is their triangle's orthocentre, which a cropped book
  photo needs; with two, the principal point is a prior (the frame's centre unless told).
* `rotation_from_vps`: the camera's rotation in the cabin's axes.
* `metric_rectifier`: the homography that turns a face lying on two cabin axes into a true front view, with the right
  aspect ratio from the camera (not assumed), scaled by one known size. This is what `photo_trace.py rectify` lacks
  when the camera is known: its affine step leaves the aspect to a second measurement.
* `plumb_k1`: one radial distortion term from curves that are straight in the world (rack edges, panel frames).
* `align_ecc`, `fuse_faces`: several rectified views (and photos) of one face aligned on the first and merged by a
  median: the halftone, the noise and the glare differ between them, the face does not.
* `panel_corners`, `room_pose`: the cabin layout's panels as 3-D rectangles, and a photo's camera from named corners
  (PnP), with each corner's residual in millimetres on its face.
* `descreen_notch`: the halftone screen taken out by notching its peaks in the spectrum, keeping the edges sharp
  (a Gaussian low-pass, `book.descreen`, softens them with the screen).
* `fit_edge`, `refine_quad`: a straight edge measured along its whole length (a step fitted across it every few
  pixels, a robust line through the steps), and a face's corners as the meeting points of its four fitted sides,
  each corner with its uncertainty: corners that are hidden or rounded come out right, because no corner is looked at.

numpy and OpenCV only; tested on synthetic data in tools/tests/test_interior.py.
"""
import math

import numpy as np


def _cv():
    import cv2
    return cv2


# ------------------------------------------------------------------------------------------------------- segments

def line_segments(gray, min_len_px=40):
    """[(x0, y0, x1, y1)] of the straight segments at least `min_len_px` long (LSD, no parameters to tune)."""
    cv2 = _cv()
    if gray.ndim == 3:
        gray = cv2.cvtColor(gray, cv2.COLOR_BGR2GRAY)
    lines = cv2.createLineSegmentDetector().detect(gray)[0]
    if lines is None:
        return np.zeros((0, 4))
    s = lines.reshape(-1, 4).astype(float)
    return s[np.hypot(s[:, 2] - s[:, 0], s[:, 3] - s[:, 1]) >= min_len_px]


def _lines_h(segs):
    a = np.column_stack([segs[:, 0], segs[:, 1], np.ones(len(segs))])
    b = np.column_stack([segs[:, 2], segs[:, 3], np.ones(len(segs))])
    L = np.cross(a, b)
    return L / np.linalg.norm(L[:, :2], axis=1, keepdims=True)


def _consistency(segs, vp):
    """Angle (rad) between each segment and the direction from its midpoint to the vanishing point."""
    mid = (segs[:, :2] + segs[:, 2:]) / 2
    d = segs[:, 2:] - segs[:, :2]
    if abs(vp[2]) < 1e-9:
        to = np.tile(vp[:2], (len(segs), 1))
    else:
        to = vp[:2] / vp[2] - mid
    cos = np.abs(np.sum(d * to, 1)) / (np.linalg.norm(d, axis=1) * np.linalg.norm(to, axis=1) + 1e-12)
    return np.arccos(np.clip(cos, 0, 1))


def _refine_vp(segs):
    L = _lines_h(segs) * np.hypot(segs[:, 2] - segs[:, 0], segs[:, 3] - segs[:, 1])[:, None]
    _, _, vt = np.linalg.svd(L)
    v = vt[-1]
    return v / np.linalg.norm(v)


def vanishing_points(segs, n=3, thr_deg=1.5, iters=600, min_inliers=6, seed=0):
    """Up to `n` vanishing points (homogeneous, unit norm) with their inlier masks, strongest first."""
    rng = np.random.default_rng(seed)
    left = np.ones(len(segs), bool)
    L = _lines_h(segs) if len(segs) else np.zeros((0, 3))
    w = np.hypot(segs[:, 2] - segs[:, 0], segs[:, 3] - segs[:, 1]) if len(segs) else np.zeros(0)
    out = []
    thr = math.radians(thr_deg)
    for _ in range(n):
        idx = np.flatnonzero(left)
        if len(idx) < min_inliers:
            break
        best, best_score = None, 0.0
        for _ in range(iters):
            i, j = rng.choice(idx, 2, replace=False)
            v = np.cross(L[i], L[j])
            if np.linalg.norm(v) < 1e-12:
                continue
            v = v / np.linalg.norm(v)
            inl = left & (_consistency(segs, v) < thr)
            score = w[inl].sum()
            if score > best_score:
                best, best_score = inl, score
        if best is None or best.sum() < min_inliers:
            break
        v = _refine_vp(segs[best])
        inl = left & (_consistency(segs, v) < thr)
        out.append((v, inl))
        left &= ~inl
    return out


# ------------------------------------------------------------------------------------------------------ intrinsics

def _finite(v, w, h, far=50.0):
    """A vanishing point's pixel position, or None if it is effectively at infinity (beyond `far` image sizes)."""
    if abs(v[2]) < 1e-12:
        return None
    p = v[:2] / v[2]
    return p if np.linalg.norm(p - (w / 2, h / 2)) < far * max(w, h) else None


def intrinsics_from_vps(vps, w, h, pp_prior=None, f_range=(0.4, 8.0), well_placed=4.0):
    """(f_px, (cx, cy), method) from mutually orthogonal vanishing points, or (None, None, reason).

    The focal length is only as good as the vanishing points' geometry: a point far outside the frame (a face seen
    nearly square-on) makes f ill-conditioned, so a pair is used only when both points lie within `well_placed`
    frame sizes of the centre, the best-placed pair wins (not the one that gives the largest f), and f must fall in
    `f_range` frame widths (a crop of a wide photo has a long equivalent focal length). A photo with no such pair is
    left uncalibrated: it is a near-frontal view, and its faces are flattened without the camera."""
    size = max(w, h)
    c0 = np.array([w / 2, h / 2])
    pts = [p for p in (_finite(v, w, h) for v in vps) if p is not None]
    ok = lambda f: f_range[0] * w <= f <= f_range[1] * w
    if len(pts) >= 3:
        a, b, c = pts[:3]
        # orthocentre: the intersection of two altitudes
        A = np.array([b - c, a - c])
        rhs = np.array([np.dot(a, b - c), np.dot(b, a - c)])
        if abs(np.linalg.det(A)) > 1e-9 and all(np.linalg.norm(p - c0) < well_placed * size for p in (a, b, c)):
            pp = np.linalg.solve(A, rhs)
            f2 = [-np.dot(p - pp, q - pp) for p, q in ((a, b), (b, c), (a, c))]
            inside = 0.1 * w < pp[0] < 0.9 * w and 0.1 * h < pp[1] < 0.9 * h
            if all(x > 0 for x in f2) and inside and ok(math.sqrt(np.mean(f2))):
                return float(math.sqrt(np.mean(f2))), (float(pp[0]), float(pp[1])), 'three vanishing points'
    if len(pts) >= 2:
        pp = np.array(pp_prior if pp_prior is not None else (w / 2, h / 2), float)
        best = None
        for i in range(len(pts)):
            for j in range(i + 1, len(pts)):
                reach = max(np.linalg.norm(pts[i] - c0), np.linalg.norm(pts[j] - c0))
                f2 = -np.dot(pts[i] - pp, pts[j] - pp)
                if reach < well_placed * size and f2 > 0 and ok(math.sqrt(f2)) and (best is None or reach < best[0]):
                    best = (reach, math.sqrt(f2))
        if best:
            return float(best[1]), (float(pp[0]), float(pp[1])), 'two vanishing points, principal point assumed'
    return None, None, 'no well-placed pair of orthogonal vanishing points (a near-frontal view)'


def K_matrix(f, pp):
    return np.array([[f, 0, pp[0]], [0, f, pp[1]], [0, 0, 1.0]])


def rotation_from_vps(K, vps):
    """Columns: the cabin axes of the vanishing points in the camera's frame, made orthonormal and right-handed.
    A third axis is completed from the first two when only two are given."""
    Ki = np.linalg.inv(K)
    cols = []
    for v in vps[:3]:
        d = Ki @ v
        cols.append(d / np.linalg.norm(d))
    if len(cols) == 2:
        cols.append(np.cross(cols[0], cols[1]))
    M = np.column_stack(cols)
    U, _, Vt = np.linalg.svd(M)
    R = U @ Vt
    if np.linalg.det(R) < 0:
        R[:, 2] = -R[:, 2]
    return R


# ------------------------------------------------------------------------------------------------- rectification

def metric_rectifier(K, r_u, r_v, corners_px, known, px_per_mm=5.0, margin_mm=20.0):
    """The homography from photo pixels to a true front view of the face spanned by the camera-frame directions r_u
    (the face's right) and r_v (its down), in px_per_mm, with a margin; and the face's size in mm.

    corners_px: the face's four corners in the photo (any order is fine; their extent sets the frame). known: {'width_mm'
    or 'height_mm': value} fixes the scale. The aspect ratio comes from the camera, not from the corners."""
    n = np.cross(r_u, r_v)
    Rp = np.vstack([r_u, r_v, n / np.linalg.norm(n)])
    H0 = Rp @ np.linalg.inv(K)  # photo px -> a fronto-parallel normalised view (up to scale)
    c = np.column_stack([np.asarray(corners_px, float), np.ones(len(corners_px))]) @ H0.T
    q = c[:, :2] / c[:, 2:3]
    lo, hi = q.min(0), q.max(0)
    span = hi - lo
    if 'width_mm' in known:
        s = known['width_mm'] / span[0]
    else:
        s = known['height_mm'] / span[1]
    size_mm = span * s
    k = px_per_mm
    S = np.array([[s * k, 0, (-lo[0] * s + margin_mm) * k], [0, s * k, (-lo[1] * s + margin_mm) * k], [0, 0, 1.0]])
    H = S @ H0
    out_px = (int(round((size_mm[0] + 2 * margin_mm) * k)), int(round((size_mm[1] + 2 * margin_mm) * k)))
    return H / H[2, 2], size_mm, out_px


def quad_aspect(corners_px, w, h, f_range=(0.5, 4.0), steps=72, cos_tol=0.02, px_sigma=1.5, pp_off=0.15):
    """The width / height of a rectangle seen as the quad tl, tr, br, bl, without knowing the camera.

    The quad's own sides give the plane's vanishing line l (v_top/bottom x v_left/right; parallel sides give a point
    at infinity, so a square-on view gives l at infinity). For any camera K the plane's normal is K^T l, the corners
    back-projected onto that plane give the rectangle, and its aspect follows. It is computed for every plausible
    focal length (`f_range` frame widths, principal point at the centre) at which the rectangle's sides meet at a right
    angle to within the tracing's accuracy (`cos_tol`, about 1 degree). A square-on face accepts every focal length
    and gives nearly the same aspect for all; a face turned a little pins the focal length and with it the aspect;
    a face turned far gives aspects that depend on the unknown principal point (a cropped print), and its spread
    says so. Returns {'aspect', 'spread', 'f_px', 'right_angle'}: the aspect at the best right angle (or the range's
    middle for a square-on face); spread is half the range of the accepted aspects, over the focal lengths and over
    principal points up to `pp_off` of the frame from its centre (the book's prints are cropped), combined with the
    corners' own error (`px_sigma` px)."""
    c = np.asarray(corners_px, float)
    X = np.column_stack([c, np.ones(4)])
    v1 = np.cross(np.cross(X[0], X[1]), np.cross(X[3], X[2]))  # top and bottom meet
    v2 = np.cross(np.cross(X[0], X[3]), np.cross(X[1], X[2]))  # left and right meet
    l = np.cross(v1, v2)

    def at(f, pp):
        K = K_matrix(f, pp)
        n = K.T @ l
        r = (np.linalg.inv(K) @ X.T).T
        P = r / (r @ n)[:, None]
        a = (np.linalg.norm(P[1] - P[0]) + np.linalg.norm(P[2] - P[3])) / (np.linalg.norm(P[3] - P[0]) + np.linalg.norm(P[2] - P[1]))
        u, v = P[1] - P[0], P[3] - P[0]
        return f, float(a), abs(float(u @ v)) / (np.linalg.norm(u) * np.linalg.norm(v))
    fs = np.geomspace(f_range[0] * w, f_range[1] * w, steps)
    out = [at(f, (w / 2, h / 2)) for f in fs]  # the centred camera gives the value
    # the cropped print's principal point may be anywhere near the middle: its range widens the spread
    others = [at(f, (w * (0.5 + dx), h * (0.5 + dy))) for f in fs for dx in (-pp_off, 0, pp_off)
              for dy in (-pp_off, 0, pp_off) if dx or dy]
    cosines = np.array([o[2] for o in out])
    i = int(np.argmin(cosines))
    # the cameras at which the sides meet at a right angle to within the tracing's accuracy: a square-on quad
    # accepts them all (and its aspect hardly changes), a turned one only a few (which pin its aspect)
    lim = max(cos_tol, cosines[i])
    ok = cosines <= lim
    asp = np.array([o[1] for o in out if o[2] <= lim] + [o[1] for o in others if o[2] <= lim])
    if ok.all():
        i = len(out) // 2
    # plus the tracing's own error: a corner off by px_sigma changes each side by about that much
    sides = [np.linalg.norm(c[1] - c[0]), np.linalg.norm(c[2] - c[3]), np.linalg.norm(c[3] - c[0]), np.linalg.norm(c[2] - c[1])]
    trace = out[i][1] * px_sigma * (2 / (sides[0] + sides[1]) + 2 / (sides[2] + sides[3]))
    spread = math.hypot((asp.max() - asp.min()) / 2, trace)
    return {'aspect': out[i][1], 'spread': float(spread), 'f_px': float(out[i][0]),
            'right_angle': bool(not ok.all())}


def rectify(img, H, out_px):
    cv2 = _cv()
    return cv2.warpPerspective(img, H, out_px, flags=cv2.INTER_LANCZOS4, borderMode=cv2.BORDER_CONSTANT)


# ------------------------------------------------------------------------------------------------------ distortion

def distort(pts, k1, w, h):
    c = np.array([w / 2, h / 2])
    s = math.hypot(w, h) / 2
    p = (pts - c) / s
    r2 = (p ** 2).sum(1, keepdims=True)
    return c + p * (1 + k1 * r2) * s


def undistort(pts, k1, w, h, iters=8):
    c = np.array([w / 2, h / 2])
    s = math.hypot(w, h) / 2
    pd = (pts - c) / s
    p = pd.copy()
    for _ in range(iters):
        r2 = (p ** 2).sum(1, keepdims=True)
        p = pd / (1 + k1 * r2)
    return c + p * s


def _straightness(chains, k1, w, h):
    err = 0.0
    for ch in chains:
        u = undistort(ch, k1, w, h)
        m = u.mean(0)
        sv = np.linalg.svd(u - m, compute_uv=False)
        err += sv[-1] ** 2 / len(u)
    return err


def plumb_k1(chains, w, h, lo=-0.15, hi=0.15, min_gain=0.3):
    """The radial term k1 (normalised by the half-diagonal) that makes world-straight point chains straightest.
    0 when it doesn't straighten them by at least `min_gain` (30 %) or lands on the search's edge: the chains are then
    curved things, not bent lines, and a lens term fitted to them would be wrong."""
    ks = np.linspace(lo, hi, 61)
    e = [_straightness(chains, k, w, h) for k in ks]
    i = int(np.argmin(e))
    e0 = _straightness(chains, 0.0, w, h)
    if i in (0, len(ks) - 1) or e[i] > (1 - min_gain) * e0:
        return 0.0
    a, b = ks[max(i - 1, 0)], ks[min(i + 1, len(ks) - 1)]
    g = (math.sqrt(5) - 1) / 2
    for _ in range(40):  # golden section inside the best grid cell
        c, d = b - g * (b - a), a + g * (b - a)
        if _straightness(chains, c, w, h) < _straightness(chains, d, w, h):
            b = d
        else:
            a = c
    return float((a + b) / 2)


def edge_chains(gray, min_len_px=150, canny=(60, 160)):
    """Long edge contours as point chains (candidates for the plumb-line fit; the caller keeps the straight-ish ones)."""
    cv2 = _cv()
    e = cv2.Canny(gray, *canny)
    cs, _ = cv2.findContours(e, cv2.RETR_LIST, cv2.CHAIN_APPROX_NONE)
    out = []
    for c in cs:
        p = c.reshape(-1, 2).astype(float)
        if len(p) >= min_len_px:
            m = p.mean(0)
            sv = np.linalg.svd(p - m, compute_uv=False)
            if sv[1] / (sv[0] + 1e-9) < 0.05:  # nearly straight already: a world line, slightly bent
                out.append(p[::3])
    return out


# ---------------------------------------------------------------------------------------------------------- fusion

def align_ecc(src, ref, iters=200, eps=1e-6):
    """src warped onto ref by a homography (enhanced correlation), or None if it does not converge."""
    cv2 = _cv()
    g = lambda im: (cv2.cvtColor(im, cv2.COLOR_BGR2GRAY) if im.ndim == 3 else im).astype(np.float32) / 255.0
    W = np.eye(3, dtype=np.float32)
    crit = (cv2.TERM_CRITERIA_EPS | cv2.TERM_CRITERIA_COUNT, iters, eps)
    a, b = g(ref), g(src)
    try:
        # coarse to fine: a quarter, a half, then full size, each level starting from the last one's warp
        for s in (0.25, 0.5, 1.0):
            S = np.diag([s, s, 1.0]).astype(np.float32)
            Ws = (S @ W @ np.linalg.inv(S)).astype(np.float32)
            ra = cv2.resize(a, None, fx=s, fy=s, interpolation=cv2.INTER_AREA) if s < 1 else a
            rb = cv2.resize(b, None, fx=s, fy=s, interpolation=cv2.INTER_AREA) if s < 1 else b
            _, Ws = cv2.findTransformECC(ra, rb, Ws, cv2.MOTION_HOMOGRAPHY, crit, None, 5 if s == 1.0 else 3)
            W = (np.linalg.inv(S) @ Ws @ S).astype(np.float32)
    except cv2.error:
        return None, None
    out = cv2.warpPerspective(src, W, (ref.shape[1], ref.shape[0]), flags=cv2.INTER_LINEAR + cv2.WARP_INVERSE_MAP)
    return out, W


def fuse_faces(faces):
    """Median of the faces aligned on the first; returns the fused face and how many joined."""
    ref = faces[0]
    stack = [ref.astype(np.float32)]
    for f in faces[1:]:
        if f.shape != ref.shape:
            f = _cv().resize(f, (ref.shape[1], ref.shape[0]), interpolation=_cv().INTER_AREA)
        a, _ = align_ecc(f, ref)
        if a is not None:
            stack.append(a.astype(np.float32))
    return np.median(np.stack(stack), axis=0).astype(ref.dtype), len(stack)


# ------------------------------------------------------------------------------------------------------- the cabin

def panel_axes(panel):
    """(n, u, v): the panel's outward normal, its right and its down, in the body frame (x right, y forward, z up).
    `facing_deg` turns the normal from +y towards +x (90 = a left-wall panel facing the aisle); `tilt_deg` leans the
    face back (its normal up)."""
    f = math.radians(panel.get('facing_deg', 0.0))
    t = math.radians(panel.get('tilt_deg', 0.0))
    n = np.array([math.sin(f) * math.cos(t), math.cos(f) * math.cos(t), math.sin(t)])
    u = np.cross(-n, [0, 0, 1.0])
    u /= np.linalg.norm(u)
    v = np.cross(n, u)  # down along the face
    v = -v if v[2] > 0 else v
    return n, u, v


def panel_corners(panel):
    """{'tl', 'tr', 'br', 'bl'}: the panel face's corners in the body frame, as seen by someone facing it."""
    _, u, v = panel_axes(panel)
    c = np.asarray(panel['centre_m'], float)
    w, h = panel['size_m']
    return {'tl': c - u * w / 2 - v * h / 2, 'tr': c + u * w / 2 - v * h / 2,
            'br': c + u * w / 2 + v * h / 2, 'bl': c - u * w / 2 + v * h / 2}


def cabin_points(cabin):
    """Named 3-D points of the layout: '<panel id>.<corner>'."""
    pts = {}
    for p in cabin.get('panels', []):
        for k, x in panel_corners(p).items():
            pts['%s.%s' % (p['id'], k)] = x
    return pts


def room_pose(points2d, cabin, w, h):
    """A photo's camera from named cabin points (PnP with an unknown focal length), and each point's residual in
    pixels and in millimetres at its depth."""
    import geometry as G
    names = [k for k in points2d if k in cabin]
    if len(names) < 6:
        return None
    P3 = np.array([cabin[k] for k in names])
    P2 = np.array([points2d[k] for k in names], float)
    cam, _ = G.pnp(P3, P2, w, h)
    if cam is None:
        return None
    proj, depth = cam.project(P3)
    res = np.linalg.norm(proj - P2, axis=1)
    mm = res * depth / cam.f * 1000.0  # a pixel's size on the face at that depth, in mm (the layout is in metres)
    return {'camera': cam.to_dict(), 'points': {k: {'px': round(float(r), 2), 'mm': round(float(m), 1)}
                                                  for k, r, m in zip(names, res, mm)},
            'rms_px': round(float(np.sqrt((res ** 2).mean())), 2)}


# ---------------------------------------------------------------------------------------------- whole-edge measuring

def descreen_notch(img, max_period_px=8.0, thr=2.0, grow=2):
    """The halftone screen removed by notches at its spectral peaks (fundamentals, harmonics and the colour
    screens' cross terms alike), the rest of the spectrum kept, so edges stay as sharp as the scan made them.

    A peak is a frequency whose log magnitude stands `thr` above its smoothed surroundings, at a period shorter than
    `max_period_px` (the screen and its harmonics; the picture's own coarser detail is never touched). The notch is
    the peak's own extent, grown by `grow` bins and feathered. Returns (the cleaned image, uint8, the input's shape;
    the number of notched peaks)."""
    cv2 = _cv()
    a = img.astype(np.float32)
    chans = [a] if a.ndim == 2 else [a[:, :, i] for i in range(a.shape[2])]
    h, w = chans[0].shape
    H, W = cv2.getOptimalDFTSize(h), cv2.getOptimalDFTSize(w)
    specs, resid = [], None
    for c in chans:
        p = cv2.copyMakeBorder(c, 0, H - h, 0, W - w, cv2.BORDER_REFLECT)
        F = np.fft.fftshift(np.fft.fft2(p))
        specs.append(F)
        L = np.log1p(np.abs(F)).astype(np.float32)
        r = L - cv2.GaussianBlur(L, (0, 0), max(3.0, min(H, W) / 150.0))
        resid = r if resid is None else np.maximum(resid, r)
    yy, xx = np.mgrid[:H, :W]
    fr = np.hypot((yy - H // 2) / H, (xx - W // 2) / W)  # cycles per pixel
    peaks = (resid > thr) & (fr > 0.8 / max_period_px)
    n_lab, _ = cv2.connectedComponents(peaks.astype(np.uint8))
    k = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (2 * grow + 1, 2 * grow + 1))
    mask = cv2.GaussianBlur(cv2.dilate(peaks.astype(np.uint8), k).astype(np.float32), (0, 0), max(1.0, grow / 2))
    keep = 1.0 - np.clip(mask, 0, 1)
    out = [np.clip(np.real(np.fft.ifft2(np.fft.ifftshift(F * keep)))[:h, :w], 0, 255) for F in specs]
    res = out[0] if len(out) == 1 else np.stack(out, axis=2)
    return res.astype(np.uint8), int(n_lab - 1)


def _sample(gray, xs, ys):
    cv2 = _cv()
    return cv2.remap(gray, xs.astype(np.float32), ys.astype(np.float32), cv2.INTER_LINEAR,
                     borderMode=cv2.BORDER_REPLICATE)


def _fit_step(s, v, mu0, w0=1.0):
    """v = a + b (1 + tanh((s - mu) / w)) / 2 by Gauss-Newton: (mu, w, a, b, rms) or None. A symmetric step gives an
    unbiased position for any symmetric blur, whatever its exact shape."""
    a, b = float(v[:3].mean()), float(v[-3:].mean() - v[:3].mean())
    mu, w = mu0, w0
    for _ in range(25):
        t = np.tanh((s - mu) / w)
        d = 1 - t * t
        J = np.column_stack([-b * d / (2 * w), -b * d * (s - mu) / (2 * w * w), np.ones_like(s), (1 + t) / 2])
        r = v - (a + b * (1 + t) / 2)
        try:
            dx = np.linalg.lstsq(J, r, rcond=None)[0]
        except np.linalg.LinAlgError:
            return None
        mu, w, a, b = mu + dx[0], max(0.2, w + dx[1]), a + dx[2], b + dx[3]
        if abs(dx[0]) < 1e-4:
            break
    t = np.tanh((s - mu) / w)
    return mu, w, a, b, float(np.sqrt(np.mean((v - (a + b * (1 + t) / 2)) ** 2)))


def _robust_line(P, c=4.685, iters=12):
    """Total-least-squares line through points with Tukey's biweight: (centre, unit direction, weights, residuals)."""
    wts = np.ones(len(P))
    for _ in range(iters):
        m = (P * wts[:, None]).sum(0) / wts.sum()
        _, _, Vt = np.linalg.svd((P - m) * np.sqrt(wts)[:, None], full_matrices=False)
        d = Vt[0]
        r = (P - m) @ np.array([-d[1], d[0]])
        u = r / (c * (1.4826 * np.median(np.abs(r)) + 1e-6))
        nw = np.where(np.abs(u) < 1, (1 - u * u) ** 2, 0.0)
        if nw.sum() < 3:
            break
        wts = nw
    m = (P * wts[:, None]).sum(0) / wts.sum()
    r = (P - m) @ np.array([-d[1], d[0]])
    return m, d, wts, r


def fit_edge(gray, spans, half_width=6.0, step=2.0, res=0.25, min_contrast=12.0, window=None):
    """A straight edge measured along its whole length.

    `spans`: [[x0, y0, x1, y1], ...], rough pieces of one straight edge (a guide within about `half_width` px of
    it). Every `step` px along them the intensity is sampled across the guide at `res` px and a smooth step fitted;
    its centre is one edge point. A robust (Tukey) line goes through the points. `window` = (lo, hi) samples from lo
    to hi px along the span's left normal (-dy, dx) instead of +-half_width: one-sided, for an edge with another step
    close beside it (a panel's edge with the frame rail just outside). Returns {'point', 'dir', 'n', 'used',
    'rms_px', 'sigma_offset_px', 'points'}, or None when fewer than 6 steps fit."""
    g = gray.astype(np.float32)
    lo, hi = window if window else (-half_width, half_width)
    s = np.arange(lo, hi + 1e-9, res)
    half_width = (hi - lo) / 2.0
    pts = []
    for x0, y0, x1, y1 in spans:
        p0, p1 = np.array([x0, y0], float), np.array([x1, y1], float)
        L = float(np.linalg.norm(p1 - p0))
        if L < 2 * step:
            continue
        d = (p1 - p0) / L
        n = np.array([-d[1], d[0]])
        for t in np.arange(step / 2, L, step):
            c = p0 + d * t
            v = _sample(g, (c[0] + s * n[0])[None, :], (c[1] + s * n[1])[None, :])[0].astype(float)
            i = int(np.argmax(np.abs(np.gradient(v))))
            fit = _fit_step(s, v, s[i])
            if not fit:
                continue
            mu, wd, _, b, rms = fit
            if abs(b) < min_contrast or not lo + 1 <= mu <= hi - 1 or wd > half_width / 2 or rms > abs(b) / 4:
                continue
            pts.append(c + n * mu)
    if len(pts) < 6:
        return None
    P = np.array(pts)
    m, d, wts, r = _robust_line(P)
    used = wts > 0
    rms = float(np.sqrt(np.average(r ** 2, weights=wts)))
    return {'point': m, 'dir': d, 'n': len(P), 'used': int(used.sum()), 'rms_px': rms,
            'sigma_offset_px': rms / math.sqrt(max(1, int(used.sum()))), 'points': P[used]}


def _meet(l1, l2):
    t = np.linalg.solve(np.column_stack([l1['dir'], -l2['dir']]), l2['point'] - l1['point'])
    return l1['point'] + t[0] * l1['dir']


SIDES = ('top', 'right', 'bottom', 'left')
CORNER_SIDES = (('top', 'left'), ('top', 'right'), ('bottom', 'right'), ('bottom', 'left'))  # tl, tr, br, bl


def _side_spans(corners, trim):
    c = np.asarray(corners, float)
    pairs = {'top': (c[0], c[1]), 'right': (c[1], c[2]), 'bottom': (c[3], c[2]), 'left': (c[0], c[3])}
    return {k: [[*(a + (b - a) * trim), *(a + (b - a) * (1 - trim))]] for k, (a, b) in pairs.items()}


def _clip_span(span, w, h, margin):
    a, b = np.array(span[:2], float), np.array(span[2:], float)
    P = a + np.outer(np.linspace(0, 1, 400), b - a)
    ok = (P[:, 0] > margin) & (P[:, 0] < w - 1 - margin) & (P[:, 1] > margin) & (P[:, 1] < h - 1 - margin)
    if ok.sum() < 3:
        return None
    i0, i1 = np.nonzero(ok)[0][[0, -1]]
    return [*P[i0], *P[i1]]


def refine_quad(gray, corners, spans=None, trim=0.08, half_width=6.0, step=2.0, boot=60, seed=0):
    """A face's corners (tl, tr, br, bl) as the meeting points of its four sides, each measured along its length.

    `corners`: rough corners (within about `half_width` px). `spans`: optional {side: [[x0, y0, x1, y1], ...]}, the
    stretches of a side that are the face's edge (a side stepped by blocks that aren't flush, or cut by the print, is
    measured only where it is the edge); by default each whole side less `trim` at both ends (corners are often
    rounded or hidden: no corner is looked at). Spans are clipped to the image.
    Returns {'corners' (4 x 2), 'sigma_px' (per corner, a bootstrap over the edge points), 'sides'} or None when a
    side can't be measured. sigma is the random part only: on the synthetic halftone test (tools/tests) the errors
    are 0.1-0.3 px with sigma about 0.1 px on a notch-descreened image; on a Gaussian-blurred one the errors are the
    same but sigma is 3-5 times too small (the blur's bias), so measure on `descreen_notch`'s output."""
    sp = _side_spans(corners, trim)
    if spans:
        sp.update({k: v for k, v in spans.items() if v})
    h, w = gray.shape[:2]
    fits = {}
    for k in SIDES:
        clipped = [c for c in (_clip_span(s, w, h, half_width + 1) for s in sp[k]) if c]
        f = fit_edge(gray, clipped, half_width, step)
        if f is None:
            return None
        fits[k] = f
    C = np.array([_meet(fits[a], fits[b]) for a, b in CORNER_SIDES])
    rng = np.random.default_rng(seed)
    S = []
    for _ in range(boot):
        bl = {}
        for k, f in fits.items():
            m, d, _, _ = _robust_line(f['points'][rng.integers(0, len(f['points']), len(f['points']))])
            bl[k] = {'point': m, 'dir': d}
        S.append([_meet(bl[a], bl[b]) for a, b in CORNER_SIDES])
    sig = np.sqrt(((np.array(S) - C[None]) ** 2).sum(2).mean(0))
    return {'corners': C, 'sigma_px': sig,
            'sides': {k: {'points': f['n'], 'used': f['used'], 'rms_px': round(f['rms_px'], 3),
                          'sigma_offset_px': round(f['sigma_offset_px'], 4)} for k, f in fits.items()}}
