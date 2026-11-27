"""The shell's section: the levelling really levels, and the readings hang together."""
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'recon'))
import shell_section as S

T = S.load()
R = {r['id']: r for r in S.interior_rows(T)}


class ShellSection(unittest.TestCase):
    def test_the_levelling_makes_the_rack_level(self):
        g = T['forward_group_pitch']
        for pair in (g['foot'], g['top']):
            a, c = [S.levelled(T, y, z) for y, z in pair]
            self.assertAlmostEqual(a, c, delta=0.005)

    def test_the_footwell_is_below_the_aisle_and_rises_to_the_toes(self):
        self.assertLess(R['footwell_mat']['photo'], -0.25)
        self.assertGreater(R['toe_floor']['photo'], R['footwell_mat']['photo'] + 0.1)
        self.assertLess(R['toe_floor']['photo'], 0.0)

    def test_the_tubes_are_at_the_aisle_floor_so_the_seats_are_not_on_a_deck(self):
        self.assertAlmostEqual(R['gap_tubes']['photo'], 0.0, delta=0.08)

    def test_the_two_profiles_agree(self):
        _, each = S.exterior_mean(T)
        for k in each[0]:
            self.assertAlmostEqual(each[0][k], each[1][k], delta=0.13, msg=k)

    def test_the_cab_goes_up_in_the_right_order(self):
        order = ['dash_foot', 'dash_top', 'screen_foot', 'screen_top', 'header']
        values = [R[k]['photo'] for k in order]
        self.assertEqual(values, sorted(values))

    def test_the_cabin_stands_where_the_photos_put_it(self):
        for r in S.interior_rows(T):
            if r['model'] is not None:
                self.assertAlmostEqual(r['model'], r['photo'], delta=0.09, msg=r['id'])

    def test_the_two_ties_to_the_exterior_give_the_deck(self):
        a, b = S.floor_from_windows(T)
        lo, hi = S.aisle_from_roof(T)
        self.assertAlmostEqual((a + b) / 2 - (lo + hi) / 2, T['datum']['deck_above_aisle_m'], delta=0.05)

    def test_the_solves_yardsticks_agree_within_three_percent(self):
        k, each = S.yardsticks(T)
        for v in each:
            self.assertAlmostEqual(v / k, 1.0, delta=0.03)

    def test_the_footwell_is_the_aisles_own_level(self):
        self.assertAlmostEqual(R['footwell_mat']['photo'], -T['datum']['deck_above_aisle_m'], delta=0.05)

    def test_the_floor_is_inside_the_hull_by_either_window_edge(self):
        mean, _ = S.exterior_mean(T)
        for floor in S.floor_from_windows(T):
            self.assertGreater(floor + R['footwell_mat']['photo'], mean['hull_lower'])


if __name__ == '__main__':
    unittest.main()
