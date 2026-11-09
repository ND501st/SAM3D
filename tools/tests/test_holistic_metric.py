"""Fusing a depth map with the solve's points and the traces (tools/recon/holistic_metric.py): cases whose true depth is known."""
import os
import sys
import unittest

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'recon'))
import holistic_metric as M  # noqa: E402

H, W = 240, 320
Y, X = np.mgrid[0:H, 0:W].astype(float)


def pins(z, n=300, seed=1, where=None):
    rng = np.random.default_rng(seed)
    x, y = rng.integers(0, W, n * 4), rng.integers(0, H, n * 4)
    if where is not None:
        k = where[y, x]
        x, y = x[k], y[k]
    x, y = x[:n], y[:n]
    return x.astype(float), y.astype(float), z[y, x]


NONE = (np.zeros(0), np.zeros(0), np.zeros(0))
TRUE = 2.0 + X / W + 0.5 * Y / H
COMPRESSED = TRUE * np.exp(-0.12 - 0.2 * X / W + 0.1 * (Y / H) ** 2)          # what a depth network does: right shape, distances squeezed unevenly


class Fuse(unittest.TestCase):
    def test_the_distances_become_the_pins_between_the_pins(self):
        z, rep = M.fuse_map(COMPRESSED, pins(TRUE), NONE)
        self.assertLess(np.median(np.abs(z - TRUE)), 0.1 * np.median(np.abs(COMPRESSED - TRUE)))
        self.assertLess(rep['holdout_after_m'], 0.25 * rep['holdout_before_m'])
        self.assertGreater(rep['held_pins'], 0)

    def test_the_map_keeps_its_own_shape_across_a_depth_edge(self):
        """A pinless object takes its neighbours' correction (nothing else is known of it), but whole: flat stays flat, the step stays sharp, and the wall beside it is fully corrected."""
        truth = np.full((H, W), 3.0)
        box = (X > 110) & (X < 210) & (Y > 70) & (Y < 170)
        truth[box] = 1.0
        z = truth * 0.8
        fused, _ = M.fuse_map(z, pins(truth, where=~box), NONE)
        self.assertLess(np.median(np.abs(fused[~box] - 3.0)), 0.03)
        inner = (X > 115) & (X < 205) & (Y > 75) & (Y < 165)
        self.assertLess(np.ptp(fused[inner]), 0.02)
        self.assertGreater(fused[120, 105] - fused[120, 115], 1.5)

    def test_trace_pins_count_as_pins(self):
        z, rep = M.fuse_map(COMPRESSED, NONE, pins(TRUE, n=600))
        self.assertEqual(rep['pins_points'], 0)
        self.assertLess(np.median(np.abs(z - TRUE)), 0.05)

    def test_a_wrong_pin_is_turned_down(self):
        x, y, t = pins(TRUE)
        bad = t.copy()
        bad[:15] *= 1.2
        clean, _ = M.fuse_map(COMPRESSED, (x, y, t), NONE)
        dirty, _ = M.fuse_map(COMPRESSED, (x, y, bad), NONE)
        self.assertLess(np.median(np.abs(dirty - clean)), 0.02)

    def test_too_few_pins_leave_the_map_alone(self):
        z, rep = M.fuse_map(COMPRESSED, pins(TRUE, n=10), NONE)
        self.assertTrue(np.array_equal(z, COMPRESSED))
        self.assertEqual(rep['skipped'], 'too few pins')
        self.assertIsNone(rep['holdout_after_m'])


class Pins(unittest.TestCase):
    def test_trace_pins_are_only_first_hits_on_traced_parts(self):
        class Cam:
            R = np.eye(3)

            def centre(self):
                return np.zeros(3)

        class Scene:
            part = np.array([0, 1])

            def cast(self, origin, d):
                tri = np.where(np.arange(len(d)) % 2 == 0, 0, 1)
                return tri, np.full(len(d), 2.0)

        rays = lambda cam, x, y: np.stack([x * 0, y * 0, x * 0 + 1], axis=-1)   # noqa: E731
        x, y, z = M.trace_pins(np.ones((64, 64)), Cam(), Scene(), np.array([True, False]), rays)
        self.assertEqual(len(x), 32)                                           # half of the 64 rays hit the untraced part
        self.assertTrue(np.all(z == 2.0))


if __name__ == '__main__':
    unittest.main()
