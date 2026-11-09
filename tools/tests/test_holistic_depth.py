"""The depth check's arithmetic and the camera read from a photo's vanishing point (docs/OSA_INTERIOR_HOLISTIC.md sections 29, 30)."""
import os
import sys
import unittest

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'recon'))
import holistic_depth as D  # noqa: E402
import holistic_view as V  # noqa: E402


class Depth(unittest.TestCase):
    def test_residual_sign(self):
        self.assertAlmostEqual(D.residual(3.0, [0, 0, 0], [0, 2, 0]), 1.0)     # the model's surface beyond the photo's point
        self.assertAlmostEqual(D.residual(1.5, [0, 0, 0], [0, 2, 0]), -0.5)

    def test_cab_group_left_out(self):
        self.assertFalse(D.compartment('g082-b'))
        self.assertTrue(D.compartment('g096-a'))

    def test_rays_one_per_compartment_sighting(self):
        cams = {k: {'R': np.eye(3).tolist(), 't': [0, 0, 0]} for k in ('g096-a', 'g082-b')}
        ry, meta, _ = D.rays(cams, [[0, 0, 5, ['g096-a', 'g082-b']]])
        self.assertEqual(meta, [('g096-a', 0)])
        self.assertEqual(ry[0][2], [0.0, 0.0, 5.0])

    def test_summary(self):
        t = D.summarise([('a', 0.01), ('a', 0.2), ('b', -0.1)])
        self.assertEqual(t[0]['part'], 'a')
        self.assertEqual(t[0]['within_band'], 0.5)


class DepthMaps(unittest.TestCase):
    """The depth maps' stages (section 31), on made-up numbers."""

    def test_scale_field_found_through_outliers(self):
        rng = np.random.default_rng(3)
        u, v = rng.uniform(-1, 1, 400), rng.uniform(-1, 1, 400)
        truth = np.array([np.log(0.5), 0.05, -0.03, 0.0, 0.02, 0.0])
        ratio = np.exp(D.poly_terms(u, v) @ truth)
        ratio[:30] *= 2.5                                     # points the map has wrong
        c, mad = D.fit_scale(u, v, ratio)
        self.assertLess(np.abs(c - truth).max(), 0.01)
        self.assertLess(mad, 0.005)

    def test_few_points_give_one_number(self):
        c, _ = D.fit_scale(np.zeros(10), np.zeros(10), np.full(10, 2.0))
        self.assertAlmostEqual(c[0], np.log(2.0))
        self.assertTrue((c[1:] == 0).all())

    def test_lift_lands_on_the_plane_the_map_says(self):
        cam = V.Camera(100.0, 50.0, 40.0, np.eye(3), np.zeros(3), 100, 80)
        X, x, y = D.lift(np.full((80, 100), 2.0), cam, 10)
        self.assertTrue(np.allclose(X[:, 2], 2.0))
        self.assertAlmostEqual(float(X[0, 0]), (x[0] - 50.0) / 100.0 * 2.0)

    def test_lift_skips_what_the_map_marks_invalid(self):
        cam = V.Camera(100.0, 50.0, 40.0, np.eye(3), np.zeros(3), 100, 80)
        z = np.full((80, 100), 2.0)
        z[:40] = np.nan
        X, _, y = D.lift(z, cam, 10)
        self.assertTrue((y >= 40).all())
        self.assertEqual(len(X), 40)

    def test_difference_map_colours(self):
        im = D.diverging(np.array([[0.0, 0.2, -0.2, np.nan]]), 0.05)[0]
        self.assertEqual(im[0].tolist(), [255, 255, 255])     # within: white
        self.assertEqual(im[1].tolist(), [255, 0, 0])         # model beyond the photo: blue (BGR)
        self.assertEqual(im[2].tolist(), [0, 0, 255])         # model in front: red
        self.assertEqual(im[3].tolist(), [120, 120, 120])


class MeasuredCamera(unittest.TestCase):
    M = {'eye_m': [0.0, 1.0, 1.8], 'f_px': 1000.0, 'vanish_x_px': 500.0, 'horizon_px': 300.0}

    def test_a_point_far_ahead_lands_on_the_vanishing_point(self):
        c = V.measured_camera(self.M, 1000, 800)
        p = c.project(np.array([0.0, 1e6, 1.8]))[0]
        self.assertAlmostEqual(float(p[0]), 500.0, places=1)
        self.assertAlmostEqual(float(p[1]), 300.0, places=1)

    def test_vanishing_point_right_of_the_middle(self):
        c = V.measured_camera(dict(self.M, vanish_x_px=600.0), 1000, 800)
        self.assertAlmostEqual(float(c.project(np.array([0.0, 1e6, 1.8]))[0][0]), 600.0, delta=1.0)   # the pitch's cosine: half a pixel here

    def test_lower_things_lower_in_the_image(self):
        c = V.measured_camera(self.M, 1000, 800)
        self.assertGreater(float(c.project(np.array([0.0, 4.0, 1.0]))[0][1]), 300.0)


if __name__ == '__main__':
    unittest.main()
