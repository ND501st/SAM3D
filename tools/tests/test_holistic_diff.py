"""The comparison that draws conclusions (tools/recon/holistic_diff.py) and the bundle adjustment under it (holistic_refine.py): what each is expected to conclude on cases worked by hand."""
import os
import sys
import unittest

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'recon'))
import holistic_diff as H  # noqa: E402
import holistic_refine as R  # noqa: E402
import holistic_view as V  # noqa: E402


def finding(**over):
    f = {'median_r_m': 0.0, 'plane_angle_deg': 0.0, 'along_normal_m': 0.0, 'share': 0.01, 'part': 'bench', 'photo_centre_m': [0.0, 0.0, 1.0], 'model_centre_m': [0.0, 0.0, 1.0], 'photo_rms_m': 0.01, 'model_rms_m': 0.01,
         'photo_normal': [0.0, 1.0, 0.0], 'model_normal': [0.0, 1.0, 0.0], 'yaw_delta_deg': 0.0, 'tilt_delta_deg': 0.0, 'surface_gap_m': 0.0}
    f.update(over)
    return f


class Classify(unittest.TestCase):
    def test_two_planes_at_different_angles_are_a_turn(self):
        kind, says = H.classify(finding(plane_angle_deg=25.0, yaw_delta_deg=25.0))
        self.assertEqual(kind, 'turned')
        self.assertIn('25 degrees', says)

    def test_a_rough_surface_is_not_called_turned(self):
        self.assertNotEqual(H.classify(finding(plane_angle_deg=25.0, photo_rms_m=0.08, along_normal_m=0.05))[0], 'turned')

    def test_hull_floor_higher_in_the_photos_is_a_raise(self):
        kind, says = H.classify(finding(part='hull', median_r_m=0.10, surface_gap_m=0.10, photo_normal=[0.0, 0.0, 1.0], model_normal=[0.0, 0.0, 1.0], photo_centre_m=[0.2, 0.6, 0.93], model_centre_m=[0.2, 0.6, 0.83]))
        self.assertEqual(kind, 'raise')
        self.assertIn('0.10', says)

    def test_something_in_front_of_the_model_is_missing_and_behind_it_excess(self):
        self.assertEqual(H.classify(finding(median_r_m=0.2, surface_gap_m=0.2))[0], 'missing')
        self.assertEqual(H.classify(finding(median_r_m=-0.2, surface_gap_m=0.2))[0], 'excess')

    def test_a_small_shift_along_the_normal_is_a_move_with_its_vector(self):
        kind, says = H.classify(finding(along_normal_m=0.04, surface_gap_m=0.04, median_r_m=0.04, model_normal=[0.0, 0.0, 1.0]))
        self.assertEqual(kind, 'moved')
        self.assertIn('z +0.040', says)


class Merge(unittest.TestCase):
    def one(self, photo, part='hull', kind='moved', cells=((0, 0, 0), (1, 0, 0), (2, 0, 0), (3, 0, 0))):
        return dict(finding(part=part), photo=photo, kind=kind, says='x', pixels=100, cells=[list(c) for c in cells], bbox_photo_m=[], n=1)

    def test_the_same_region_from_two_views_is_one_finding(self):
        m = H.merge_findings([self.one('a'), self.one('b')])
        self.assertEqual(len(m), 1)
        self.assertEqual(m[0]['views'], ['a', 'b'])
        self.assertEqual(m[0]['id'], 'F001')

    def test_other_part_or_kind_or_place_stays_apart(self):
        far = ((30, 0, 0), (31, 0, 0), (32, 0, 0), (33, 0, 0))
        self.assertEqual(len(H.merge_findings([self.one('a'), self.one('b', part='bench')])), 2)
        self.assertEqual(len(H.merge_findings([self.one('a'), self.one('b', kind='turned')])), 2)
        self.assertEqual(len(H.merge_findings([self.one('a'), self.one('b', cells=far)])), 2)

    def test_two_regions_in_one_photo_are_not_merged(self):
        self.assertEqual(len(H.merge_findings([self.one('a'), self.one('a')])), 2)


class Planes(unittest.TestCase):
    def test_ransac_finds_the_plane_through_outliers(self):
        rng = np.random.default_rng(3)
        n = np.array([0.3, 0.9, 0.2])
        n /= np.linalg.norm(n)
        u = np.cross(n, [0, 0, 1.0])
        u /= np.linalg.norm(u)
        v = np.cross(n, u)
        P = np.array([1.0, 2.0, 3.0]) + np.outer(rng.uniform(-1, 1, 300), u) + np.outer(rng.uniform(-1, 1, 300), v)
        P = np.vstack([P, rng.uniform(-3, 3, (60, 3))])
        c, m, k = H.ransac_plane(P)
        self.assertGreater(abs(np.dot(m, n)), 0.9999)
        self.assertGreater(k[:300].mean(), 0.97)

    def test_plane_words_read_yaw_as_the_builder_turns(self):
        """A front built facing +y and turned by -38 about z faces (-sin 38, cos 38), which the comparison reads as yaw +38."""
        a = np.radians(38)
        n = np.array([-np.sin(a), np.cos(a), 0.0])
        u = np.array([np.cos(a), np.sin(a), 0.0])
        g = np.linspace(-0.3, 0.3, 25)
        P = np.array([u * p + np.array([0, 0, 1.0]) * q + n * 1.5 for p in g for q in g])
        self.assertAlmostEqual(H.plane_words(P)['yaw_deg'], 38.0, delta=0.2)


class FakeScene:
    """A room of flat walls, each named as a traced part so registration may use it. A corner alone cannot tell the map's scale from the eye's place; opposite walls can."""
    names = ['rack_a']
    part = np.zeros(4, int)
    normal = np.array([[0, 1.0, 0], [1.0, 0, 0], [-1.0, 0, 0], [0, 0, 1.0]])
    offsets = np.array([-3.0, -1.2, -1.2, -0.8])          # n . X = offset

    def cast(self, origin, dirs):
        with np.errstate(divide='ignore', invalid='ignore'):
            ts = (self.offsets[:, None] - self.normal @ origin[:, None]) / (dirs @ self.normal.T).T
        ts = ts.T
        ts[~np.isfinite(ts) | (ts <= 0)] = np.inf
        tri = np.argmin(ts, axis=1)
        t = ts[np.arange(len(dirs)), tri]
        tri[~np.isfinite(t)] = -1
        return tri, t


def looking_aft(w=320, h=240, f=260.0, eye=(0.0, 0.0, 0.0)):
    R = np.array([[-1.0, 0, 0], [0, 0, -1.0], [0, -1.0, 0]])          # camera x = -body x, y = -body z, z = -body y: an aft view
    eye = np.array(eye, float)
    return V.Camera(f, w / 2, h / 2, R, -R @ eye, w, h)


def depth_of(cam, scene):
    y, x = np.mgrid[0:cam.h, 0:cam.w]
    c = H.rays_of(cam, x.ravel() + 0.5, y.ravel() + 0.5)
    nrm = np.linalg.norm(c, axis=1)
    _, t = scene.cast(cam.centre(), (cam.R.T @ (c / nrm[:, None]).T).T)
    return (t / nrm).reshape(cam.h, cam.w)


class Registration(unittest.TestCase):
    def test_moved_camera_returns_onto_the_model(self):
        scene = FakeScene()
        truth = looking_aft()
        z = depth_of(truth, scene)
        start = H.camera_moved(truth, np.radians([1.0, -1.0, 0.5]), np.array([0.04, -0.03, 0.05]), 0.0)
        cam, s, on = H.icp_view(start, z, scene, None, 2, 24, anchored=np.array([True]))
        self.assertGreater(on, 0.97)
        self.assertLess(np.linalg.norm(cam.centre() - truth.centre()), 0.02)
        self.assertAlmostEqual(s, 1.0, delta=0.02)

    def test_a_registration_stays_a_correction_of_the_solve(self):
        cam = looking_aft()
        self.assertTrue(H.within_limits(H.camera_moved(cam, np.radians([0.0, 1.0, 0.0]), np.array([0.1, 0.0, 0.05]), 0.0), cam))
        self.assertFalse(H.within_limits(H.camera_moved(cam, np.zeros(3), np.array([0.4, 0.0, 0.0]), 0.0), cam))     # g092-a's eye went 0.56 m
        self.assertFalse(H.within_limits(H.camera_moved(cam, np.radians([0.0, 12.0, 0.0]), np.zeros(3), 0.0), cam))   # and turned 20 degrees

    def test_map_scale_is_kept_in_bounds(self):
        scene = FakeScene()
        cam = looking_aft()
        _, s, _ = H.icp_view(cam, depth_of(cam, scene) * 0.5, scene, None, 3, 12, anchored=np.array([True]))
        self.assertLessEqual(s, 1.08 + 1e-9)


class BundleAdjustment(unittest.TestCase):
    def setup_case(self):
        rng = np.random.default_rng(5)
        X = rng.uniform([-1, -1, 2], [1, 1, 4], (120, 3))
        C = np.zeros((6, 8))
        C[:, 3] = np.linspace(-0.6, 0.6, 6)
        C[:, 1] = np.linspace(-0.1, 0.1, 6)
        C[:, 6] = np.log(800.0)
        oc = np.repeat(np.arange(6), len(X))
        op = np.tile(np.arange(len(X)), 6)
        return rng, C, X, oc, op, R.project(C, X[op], oc)

    def test_recovers_cameras_and_points_held_by_an_anchor(self):
        rng, C, X, oc, op, obs = self.setup_case()
        C0 = C + rng.normal(0, [0.005, 0.005, 0.005, 0.01, 0.01, 0.01, 0.0, 0.0], C.shape)
        X0 = X + rng.normal(0, 0.01, X.shape)
        anchor = np.arange(30)
        C1, X1, hist = R.bundle_adjust(C0, X0, oc, op, obs, anchor, X[anchor], focal=np.full(6, np.log(800.0)))
        self.assertLess(hist[-1], hist[0] * 0.01 + 1.0)
        self.assertLess(np.median(np.linalg.norm(X1 - X, axis=1)), 0.005)
        self.assertLess(np.max(np.abs(C1[:, 3:6] - C[:, 3:6])), 0.02)

    def test_a_plane_prior_moves_the_points_it_names(self):
        rng, C, X, oc, op, obs = self.setup_case()
        idx = np.arange(40)
        C1, X1, _ = R.bundle_adjust(C.copy(), X.copy(), oc, op, obs, np.arange(40, 90), X[40:90], planes=[(idx, 2, X[idx, 2] + 0.02, 0.002)])
        self.assertAlmostEqual(float(np.median(X1[idx, 2] - X[idx, 2])), 0.02, delta=0.01)

    def test_a_robust_loss_survives_a_wild_observation(self):
        rng, C, X, oc, op, obs = self.setup_case()
        obs = obs.copy()
        obs[5] += 300.0
        C1, X1, _ = R.bundle_adjust(C.copy(), X.copy(), oc, op, obs, np.arange(40), X[:40])
        self.assertLess(np.median(np.linalg.norm(X1 - X, axis=1)), 0.003)


if __name__ == '__main__':
    unittest.main()
