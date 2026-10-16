"""tools/photo_trace.py's geometry: a plate seen in perspective is flattened back square, and circles are found where they
are. Synthetic data with a known answer (skipped without numpy and OpenCV)."""
import math
import sys
from pathlib import Path

import pytest

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools"))
np = pytest.importorskip("numpy")
cv2 = pytest.importorskip("cv2")
import photo_trace as PT  # noqa: E402


def _seen(H, pts):
    p = cv2.perspectiveTransform(np.array([pts], np.float64), H)[0]
    return p


def test_flattening_undoes_a_perspective_view():
    # a 400 x 200 plate seen through a known homography; lines along its rows and columns
    H = np.array([[0.9, 0.08, 120.0], [-0.03, 1.05, 80.0], [0.00012, 0.00021, 1.0]])
    lines = {}
    for i, y in enumerate((10.0, 90.0, 190.0)):
        a, b = _seen(H, [(0.0, y), (400.0, y)])
        d = (b - a) / np.linalg.norm(b - a)
        lines["h%d" % i] = (a, d, 1, 1)
    for i, x in enumerate((15.0, 210.0, 385.0)):
        a, b = _seen(H, [(x, 0.0), (x, 200.0)])
        d = (b - a) / np.linalg.norm(b - a)
        lines["v%d" % i] = (a, d, 1, 1)
    P, _, _ = PT.projective(lines)
    corners = cv2.perspectiveTransform(np.array([_seen(H, [(0, 0), (400, 0), (400, 200), (0, 200)])]), P)[0]
    w = np.linalg.norm(corners[1] - corners[0])
    h = np.linalg.norm(corners[3] - corners[0])
    # a rectangle again: right angles, edges parallel to the axes, and (with no further help) the right aspect only
    # up to the affine part the vanishing points can't fix, which the plate's circles check in practice
    e1, e2 = corners[1] - corners[0], corners[3] - corners[0]
    assert abs(e1[1]) < 1e-6 * w and abs(e2[0]) < 1e-6 * h
    assert np.allclose(corners[2], corners[1] + e2, atol=1e-6 * w)


def test_circle_fit_finds_a_partly_hidden_rim():
    rng = np.random.RandomState(3)
    t = rng.uniform(0.3, 2 * math.pi - 0.3, 600)         # most of the rim, a gap where a knob hides it
    xs = 250 + 80 * np.cos(t) + rng.normal(0, 0.6, t.size)
    ys = 140 + 80 * np.sin(t) + rng.normal(0, 0.6, t.size)
    xs = np.r_[xs, rng.uniform(150, 350, 300)]           # clutter inside and round it
    ys = np.r_[ys, rng.uniform(40, 240, 300)]
    cx, cy, r, cov = PT.fit_circle((xs, ys), 245.0, 145.0, 75.0, np)
    assert abs(cx - 250) < 0.5 and abs(cy - 140) < 0.5 and abs(r - 80) < 0.5
    assert cov > 0.5
