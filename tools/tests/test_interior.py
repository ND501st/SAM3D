"""Tests for the interior pipeline's algorithms (tools/recon/interior.py) and its orchestrator: synthetic cabins with a
known camera, so they run without the book's scans."""
import json
import math
import os
import sys

import pytest

np = pytest.importorskip('numpy')
cv2 = pytest.importorskip('cv2')

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'recon'))
import interior as I  # noqa: E402
import osa_interior as OI  # noqa: E402


def rot(yaw, pitch, roll=0.0):
    cy, sy = math.cos(yaw), math.sin(yaw)
    cp, sp = math.cos(pitch), math.sin(pitch)
    cr, sr = math.cos(roll), math.sin(roll)
    Rz = np.array([[cy, -sy, 0], [sy, cy, 0], [0, 0, 1]])
    Rx = np.array([[1, 0, 0], [0, cp, -sp], [0, sp, cp]])
    Rr = np.array([[cr, 0, sr], [0, 1, 0], [-sr, 0, cr]])
    return Rr @ Rx @ Rz


def camera(f=900.0, pp=(700.0, 420.0), yaw=0.5, pitch=0.25, roll=0.05):
    """World (x right, y forward, z up) -> a camera looking along +y, turned by yaw/pitch/roll."""
    base = np.array([[1, 0, 0], [0, 0, -1], [0, 1, 0.0]])  # world y -> camera z, world z -> camera -y
    R = base @ rot(yaw, pitch, roll)
    K = I.K_matrix(f, pp)
    return K, R


def project(K, R, t, X):
    Xc = X @ R.T + t
    p = Xc @ K.T
    return p[:, :2] / p[:, 2:3]


def room_segments(K, R, t, rng, w=1200, h=900, n=60, noise=0.3):
    """Segments of edges along the three world axes inside a 2.6 x 6 x 1.8 m box seen from inside."""
    segs = []
    for axis in range(3):
        for _ in range(n):
            a = rng.uniform([-1.3, 1.0, 0.6], [1.3, 6.0, 2.1])
            b = a.copy()
            b[axis] += rng.uniform(0.3, 0.8) * (1 if rng.random() < 0.5 else -1)
            p = project(K, R, t, np.array([a, b]))
            if (p.min() < 0) or (p[:, 0].max() > w) or (p[:, 1].max() > h):
                continue
            if (np.vstack([a, b]) @ R.T + t)[:, 2].min() < 0.3:
                continue
            segs.append(p.ravel() + rng.normal(0, noise, 4))
    return np.array(segs)


def test_focal_and_principal_point_from_three_vanishing_points():
    rng = np.random.default_rng(3)
    K, R = camera()
    t = -R @ np.array([0.0, -0.5, 1.6])
    segs = room_segments(K, R, t, rng)
    vps = [v for v, _ in I.vanishing_points(segs, n=3)]
    assert len(vps) == 3
    f, pp, how = I.intrinsics_from_vps(vps, 1200, 900)
    assert how.startswith('three')
    assert f == pytest.approx(900, rel=0.04)
    assert pp[0] == pytest.approx(700, abs=40) and pp[1] == pytest.approx(420, abs=40)  # a cropped print: off centre


def test_rotation_from_vanishing_points_matches_the_camera():
    K, R = camera()
    vps = [K @ R[:, i] for i in range(3)]
    Rf = I.rotation_from_vps(K, vps)
    for i in range(3):  # each axis up to its sign
        assert abs(abs(Rf[:, i] @ R[:, i]) - 1) < 1e-6


def test_metric_rectifier_gets_the_aspect_from_the_camera():
    """A 600 x 400 mm panel on the left wall (x = const), seen obliquely: the flattened front is 600 x 400."""
    K, R = camera(yaw=-0.7, pitch=0.1)
    t = -R @ np.array([0.4, 0.0, 1.5])
    # the panel: right along +y (forward), down along -z, at x = -0.7
    c = np.array([-0.7, 1.6, 1.5])
    u, v = np.array([0, 1.0, 0]), np.array([0, 0, -1.0])
    corners = np.array([c - u * 0.3 - v * 0.2, c + u * 0.3 - v * 0.2, c + u * 0.3 + v * 0.2, c - u * 0.3 + v * 0.2])
    px = project(K, R, t, corners)
    H, size_mm, _ = I.metric_rectifier(K, R @ u, R @ v, px, {'width_mm': 600.0}, px_per_mm=2.0, margin_mm=10)
    assert size_mm[0] == pytest.approx(600, rel=1e-6)
    assert size_mm[1] == pytest.approx(400, rel=0.002)
    q = np.column_stack([px, np.ones(4)]) @ H.T
    q = q[:, :2] / q[:, 2:3]
    assert np.allclose(q[0], [20, 20], atol=0.5) and np.allclose(q[2], [20 + 1200, 20 + 800], atol=0.5)


def test_plumb_line_recovers_the_radial_term():
    rng = np.random.default_rng(1)
    w, h, k1 = 1600, 1200, -0.08
    chains = []
    for _ in range(8):
        a = rng.uniform([100, 100], [1500, 1100])
        ang = rng.uniform(0, math.pi)
        s = np.linspace(-500, 500, 120)
        p = a + np.outer(s, [math.cos(ang), math.sin(ang)])
        p = p[(p[:, 0] > 0) & (p[:, 0] < w) & (p[:, 1] > 0) & (p[:, 1] < h)]
        if len(p) > 40:
            chains.append(I.distort(p, k1, w, h) + rng.normal(0, 0.2, p.shape))
    assert I.plumb_k1(chains, w, h) == pytest.approx(k1, abs=0.01)


def textured(n=400, seed=0):
    rng = np.random.default_rng(seed)
    img = cv2.resize(rng.integers(0, 255, (n // 8, n // 8), dtype=np.uint8), (n, n), interpolation=cv2.INTER_CUBIC)
    for i in range(0, n, 50):
        cv2.circle(img, (i + 25, (i * 7) % n), 12, 255, 2)
    return cv2.GaussianBlur(img, (3, 3), 0)


def test_fusion_aligns_views_and_beats_any_one_of_them():
    truth = textured()
    rng = np.random.default_rng(2)
    views = [truth.astype(float) + rng.normal(0, 25, truth.shape)]
    for k in range(4):
        Hs = np.array([[1, 0.01 * k, 2.0 * k], [-0.008 * k, 1, -1.5 * k], [1e-5 * k, 0, 1]])
        v = cv2.warpPerspective(truth, Hs, truth.shape[::-1]).astype(float) + rng.normal(0, 25, truth.shape)
        views.append(v)
    views = [np.clip(v, 0, 255).astype(np.uint8) for v in views]
    fused, joined = I.fuse_faces(views)
    assert joined == 5
    inner = (slice(40, -40), slice(40, -40))
    err = lambda im: np.abs(im[inner].astype(float) - truth[inner]).mean()
    assert err(fused) < 0.75 * err(views[0])


def test_panel_axes_follow_the_layout_conventions():
    n, u, v = I.panel_axes({'facing_deg': 90})  # a left-wall panel faces the aisle (+x); its right is forward
    assert np.allclose(n, [1, 0, 0]) and np.allclose(u, [0, 1, 0]) and np.allclose(v, [0, 0, -1])
    n, u, _ = I.panel_axes({'facing_deg': -90})  # right wall
    assert np.allclose(n, [-1, 0, 0]) and np.allclose(u, [0, -1, 0])
    c = I.panel_corners({'centre_m': [-0.7, 1.5, 1.6], 'facing_deg': 90, 'size_m': [1.0, 0.5]})
    assert np.allclose(c['tl'], [-0.7, 1.0, 1.85]) and np.allclose(c['br'], [-0.7, 2.0, 1.35])


def test_room_pose_from_named_panel_corners():
    cabin_file = os.path.join(OI.REPO, 'data', 'cabins', 'osa_ak.json')
    cabin = I.cabin_points(json.load(open(cabin_file, encoding='utf-8')))
    names = [k for k in cabin if k.split('.')[0] in ('so_panel', 'op_panel', 'fo_panel', 'gyro_panel', 'sp2m_panel', 'sl3m_panel')]
    eye, target = np.array([0.4, 2.9, 1.7]), np.array([-0.73, 1.3, 1.5])  # in the aisle, looking aft-left at SŁ-2M
    z = (target - eye) / np.linalg.norm(target - eye)
    x = np.cross(z, [0, 0, 1.0])
    x /= np.linalg.norm(x)
    R = np.vstack([x, np.cross(z, x), z])
    K = I.K_matrix(1000, (800, 600))
    t = -R @ eye
    X = np.array([cabin[k] for k in names])
    p = project(K, R, t, X)
    keep = [i for i, q in enumerate(p) if 0 < q[0] < 1600 and 0 < q[1] < 1200 and (X[i] @ R.T + t)[2] > 0.2]
    pts = {names[i]: p[i] for i in keep}
    assert len(pts) >= 6
    out = I.room_pose(pts, cabin, 1600, 1200)
    assert out['rms_px'] < 1.0
    assert out['camera']['f'] == pytest.approx(1000, rel=0.05)


def test_quad_aspect_without_a_camera():
    w, h = 1600, 1200
    sq = I.quad_aspect([[100, 100], [700, 102], [699, 501], [101, 499]], w, h)
    assert sq['aspect'] == pytest.approx(1.5, rel=0.01) and sq['spread'] < 0.01
    # a 1.8 x 1.2 m rack turned 7 degrees and seen from below at 1.6 m, on a cropped print: within 1 %, and the spread
    # (the unknown principal point) under the stage's 5 % gate
    K, R = camera(f=1400.0, pp=(700.0, 520.0), yaw=0.12, pitch=0.06, roll=0.0)
    t = -R @ np.array([0.0, -1.6, 1.4])
    rect = np.array([[-0.9, 0, 2.0], [0.9, 0, 2.0], [0.9, 0, 0.8], [-0.9, 0, 0.8]])
    q = I.quad_aspect(project(K, R, t, rect), w, h)
    assert q['aspect'] == pytest.approx(1.5, rel=0.01) and q['spread'] < 0.05 * q['aspect']
    # from far round to the side on a cropped print (principal point off centre): the aspect is off by 12 %, and the
    # spread says it can't be trusted, so the faces stage refuses it (over 5 %)
    K, R = camera(f=1400.0, pp=(680.0, 500.0), yaw=0.7, pitch=0.1, roll=0.0)
    t = -R @ np.array([1.0, -1.2, 1.4])
    q = I.quad_aspect(project(K, R, t, rect), w, h)
    assert q['spread'] > 0.05 * q['aspect'] and abs(q['aspect'] - 1.5) < q['spread']


def test_two_vanishing_points_refuse_an_ill_placed_pair():
    w, h = 1200, 900
    far = [np.array([6e4, 450.0, 1.0]), np.array([600.0, -5e4, 1.0])]  # a near square-on view: both points far out
    f, pp, how = I.intrinsics_from_vps(far, w, h)
    assert f is None and 'near-frontal' in how
    K, R = camera()
    vps = [K @ R[:, i] for i in range(2)]
    f, _, how = I.intrinsics_from_vps(vps, w, h, pp_prior=(700.0, 420.0))
    assert how.startswith('two') and f == pytest.approx(900, rel=1e-6)


# ------------------------------------------------------------------------------------------ whole-edge measuring

def scene(halftone=True, occlude=True, seed=0, w=900, h=700, period=3.0):
    """A pale face on a dark recess in perspective, anti-aliased (rendered 4x and averaged), blurred like a lens,
    then printed through a 45-degree dot screen of `period` px and scanned (blur, noise). Returns (8-bit image,
    the true corners tl, tr, br, bl)."""
    rng = np.random.default_rng(seed)
    S = 4
    Hm = np.array([[1.0, 0.04, 0.0], [-0.03, 1.0, 0.0], [1.2e-4, 0.6e-4, 1.0]])
    src = np.array([[120.0, 110.0], [760.0, 110.0], [760.0, 560.0], [120.0, 560.0]])
    true = cv2.perspectiveTransform(src[None], Hm)[0]
    # exact coverage: every pixel (centre at its integer index) sampled S x S times, each sample mapped back
    # through the homography into the face's rectangle
    off = (np.arange(S) + 0.5) / S - 0.5
    yy, xx = np.mgrid[:h, :w].astype(float)
    Hi = np.linalg.inv(Hm)
    cover = np.zeros((h, w))
    for dy in off:
        for dx in off:
            X, Y = xx + dx, yy + dy
            z = Hi[2, 0] * X + Hi[2, 1] * Y + Hi[2, 2]
            u, v = (Hi[0, 0] * X + Hi[0, 1] * Y + Hi[0, 2]) / z, (Hi[1, 0] * X + Hi[1, 1] * Y + Hi[1, 2]) / z
            cover += (u > src[0, 0]) & (u < src[1, 0]) & (v > src[0, 1]) & (v < src[2, 1])
    img = 60 + (215 - 60) * cover / (S * S)
    if occlude:  # a cable over the top-right corner and a knob near the left side
        cv2.circle(img, tuple(int(v) for v in true[1]), 14, 30, -1)
        cv2.circle(img, (int(true[0][0]) + 40, int((true[0][1] + true[3][1]) / 2)), 10, 120, -1)
    img = cv2.GaussianBlur(img, (0, 0), 1.0)
    if halftone:
        yy, xx = np.mgrid[:h, :w].astype(float)
        u, v = (xx + yy) / math.sqrt(2), (xx - yy) / math.sqrt(2)
        screen = 0.5 + 0.25 * (np.cos(2 * math.pi * u / period) + np.cos(2 * math.pi * v / period))
        img = np.where(img / 255.0 > screen, 235.0, 25.0)
        img = cv2.GaussianBlur(img, (0, 0), 0.6)
    img = img + rng.normal(0, 4, img.shape)
    return np.clip(img, 0, 255).astype(np.uint8), true


def test_fit_edge_finds_a_blurred_line_to_a_twentieth_of_a_pixel():
    img, true = scene(halftone=False, occlude=False)
    guide = (true[0] + np.array([2.5, -2.0]), true[1] + np.array([-1.0, 2.5]))  # a rough guide, 2-3 px off
    f = I.fit_edge(img, [[*guide[0], *guide[1]]])
    n = np.array([-f['dir'][1], f['dir'][0]])
    for p in (true[0] + 0.3 * (true[1] - true[0]), true[0] + 0.7 * (true[1] - true[0])):
        assert abs((p - f['point']) @ n) < 0.05


def test_notch_descreen_removes_the_screen_and_keeps_edges_sharp():
    import book
    img, true = scene(occlude=False)
    notched, peaks = I.descreen_notch(img)
    blurred = book.descreen(img, 3.0, out_scale=1.0)
    assert peaks >= 2

    def screen_power(a):  # the spectrum at the screen's fundamental (45 degrees, period 3 px), in a patch
        p = a[200:456, 250:506].astype(float)
        F = np.abs(np.fft.fft2(p - p.mean()))
        k = int(round(256 / 3.0 / math.sqrt(2)))
        return F[k - 2:k + 3, k - 2:k + 3].max()
    assert screen_power(notched) < 0.1 * screen_power(img)

    def width(a):  # 10-90 % rise across the left side, in px
        y = int((true[0][1] + true[3][1]) / 2)
        x0 = int(true[0][0] + (true[3][0] - true[0][0]) * (y - true[0][1]) / (true[3][1] - true[0][1]))
        v = cv2.GaussianBlur(a.astype(float), (1, 9), 0)[y, x0 - 10:x0 + 10]
        lo, hi = v[:4].mean(), v[-4:].mean()
        f = (v - lo) / (hi - lo)
        return float(np.argmax(f > 0.9) - np.argmax(f > 0.1))
    assert width(notched) < width(blurred)


def test_refine_quad_puts_hidden_corners_within_a_third_of_a_pixel():
    img, true = scene()
    clean, _ = I.descreen_notch(img)
    rough = true + np.random.default_rng(1).uniform(-3, 3, true.shape)
    r = I.refine_quad(clean, rough)
    err = np.linalg.norm(r['corners'] - true, axis=1)
    assert err.max() < 0.33, err  # the top-right corner is under a cable; none is looked at
    assert (r['sigma_px'] < 0.3).all() and (r['sigma_px'] > 0).all()
    rough_err = np.linalg.norm(rough - true, axis=1)
    assert err.mean() < 0.2 * rough_err.mean()
