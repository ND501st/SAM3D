"""The cab seat's trace: parts inside the outline, the hoop clear of the boss, the arms and rod consistent."""
import math
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'blender'))
import cab_seat_trace as T


class SeatTrace(unittest.TestCase):
    def test_the_back_is_square_and_as_wide_as_fig3s_seat(self):
        self.assertEqual(T.W_MM, T.H_MM)
        self.assertTrue(480 <= T.W_MM <= 530)

    def test_the_boss_sits_on_the_pan(self):
        x0, y0, x1, y1 = T.BOSS
        self.assertTrue(0 < x0 < x1 < T.W_MM and 0 < y0 < y1 < T.H_MM)
        self.assertTrue(x0 < T.WING[0] < x1 and y0 < T.WING[1] < y1)

    def test_the_hoop_is_centred_and_stands_clear_of_the_boss(self):
        h = T.HOOP
        mid = (h[0][0] + h[-1][0]) / 2
        self.assertLess(abs(mid - T.W_MM / 2), 12.0)
        # seen square-on the hoop's top touches the boss's foot: that is parallax, the hoop being out of the pan's plane
        self.assertGreater(T.Z['hoop'] - T.HOOP_R_MM, T.Z['boss'])

    def test_the_hoop_reaches_the_lower_lugs(self):
        lows = [l for l in T.LUGS if l[1] > 300]
        self.assertEqual(len(lows), 2)
        for l in lows:
            d = min(math.hypot(l[0] - p[0], l[1] - p[1]) for p in T.HOOP)
            self.assertLess(d, 15.0)

    def test_lugs_lie_near_the_hoop_and_on_the_pan(self):
        for l in T.LUGS:
            self.assertTrue(0 <= l[0] <= T.W_MM and 0 <= l[1] <= T.H_MM)
            d = min(math.hypot(l[0] - p[0], l[1] - p[1]) for p in T.HOOP)
            self.assertLess(d, 30.0, l)

    def test_the_rivets_stand_on_the_rim_and_do_not_touch(self):
        r = T.rivets()
        self.assertEqual(len(r), 24)
        for i, a in enumerate(r):
            self.assertTrue(0 < a[0] < T.W_MM and 0 < a[1] < T.H_MM)
            for b in r[i + 1:]:
                self.assertGreater(math.hypot(a[0] - b[0], a[1] - b[1]), 2 * T.RIVET_R_MM)

    def test_the_arms_and_rod(self):
        self.assertLess(T.ARM_TOP_MM, T.H_MM)
        self.assertLess(T.H_MM, T.PIVOT_Y_MM)
        self.assertLess(T.PIVOT_Y_MM, T.ARM_FOOT_MM)
        self.assertTrue(T.ARMS_X[0] < T.ROD[0] and T.ROD[1] < T.ARMS_X[1])
        self.assertGreater(T.ROD[2], T.H_MM)

    def test_the_bag_hangs_on_the_hoop(self):
        x0, y0, x1, y1 = T.BAG
        top = min(y for x, y in T.HOOP if x0 < x < x1)
        self.assertLess(abs(y0 - top), 45.0)
        fx0, fy0, fx1, fy1 = T.FLAP
        self.assertTrue(x0 < fx0 < fx1 < x1 and y0 <= fy0 < fy1 < y1)
        sx0, sy0, sx1, sy1 = T.STRAP
        self.assertTrue(fx0 < sx0 < sx1 < fx1 and sy0 <= fy1)

    def test_the_depths_order(self):
        Z = T.Z
        self.assertTrue(Z['pad_front'] < Z['pad_rear'] < Z['pan_rear'] < Z['pan_rim'] < Z['lug'] < Z['hoop'] < Z['bag_rear'])
        self.assertLess(Z['boss'], Z['hoop'] - T.HOOP_R_MM)


if __name__ == '__main__':
    unittest.main()
