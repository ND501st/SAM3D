"""The cab's floor: the step as measured, the deck between its neighbours, nothing where the feet and the seats need to be."""
import json
import os
import re
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'blender'))
import cab_floor_trace as F

ROOT = os.path.join(HERE, '..', '..')
with open(os.path.join(ROOT, 'tools', 'blender', 'build_cabin_interior.py'), encoding='utf-8') as f:
    BUILDER = f.read()
with open(os.path.join(ROOT, 'data', 'cabins', 'osa_ak.json'), encoding='utf-8') as f:
    CABIN = json.load(f)


def constant(name):
    return float(re.search(r'^%s = ([0-9.]+)' % name, BUILDER, re.M).group(1))


def shell(part):
    s = next(p for p in CABIN['shell'] if p['id'] == part)
    return [c - z / 2 for c, z in zip(s['centre_m'], s['size_m'])], [c + z / 2 for c, z in zip(s['centre_m'], s['size_m'])]


class CabFloor(unittest.TestCase):
    def test_the_step_is_the_measured_one(self):
        self.assertTrue(0.35 <= F.STEP_M <= 0.40)
        self.assertAlmostEqual(F.TOE_RISE_M, 0.42 - 0.24, delta=0.01)

    def test_the_tubes_sit_at_the_measured_height(self):
        for _, dz in F.TUBES:
            self.assertAlmostEqual(F.STEP_M + dz, F.TUBE_ABOVE_MAT_M, delta=0.05)

    def test_the_seats_rails_clear_the_cushion_frame_on_the_deck(self):
        stand = constant('SEAT_Z') + 0.07 - (constant('FLOOR_Z') + F.STEP_M)
        self.assertGreaterEqual(stand, 0.116 + 0.022)    # the cushion frame's underside, then the rail on the deck

    def test_the_deck_lies_between_its_neighbours(self):
        lo, hi = shell('radio_left')
        self.assertLess(hi[0], F.DECK_X[0])   # the left radio's well is outboard of the deck
        self.assertLess(F.DECK_FORE_Y, shell('rack_sp1m')[1][1])
        self.assertLess(shell('r124_a4')[1][0], F.DECK_X[0] + 0.02)
        self.assertGreater(shell('r124_a1')[0][0], F.DECK_X[1] - 0.02)

    def test_the_mat_lies_on_the_deck_and_ends_at_the_seats_arms(self):
        self.assertTrue(F.DECK_X[0] < F.MAT_X[0] < F.MAT_X[1] < F.DECK_X[1])
        self.assertTrue(F.DECK_AFT_Y < F.MAT_Y[0] < F.MAT_Y[1] < F.GAP_AFT_Y)
        for x, y in F.MAT_BOLTS:
            self.assertTrue(F.MAT_X[0] < x < F.MAT_X[1] and F.MAT_Y[0] < y < F.MAT_Y[1])

    def test_the_frame_opens_over_the_gap(self):
        cx, _, w, _, _ = F.FRAME_HOLE
        self.assertAlmostEqual(cx - w / 2, F.GAP_X[0], delta=0.005)
        self.assertAlmostEqual(cx + w / 2, F.GAP_X[1], delta=0.005)
        self.assertTrue(F.FRAME_X[0] < cx - w / 2 and cx + w / 2 < F.FRAME_X[1])

    def test_the_pedals_stand_clear_of_the_toe_slope(self):
        lo, hi = shell('pedals')
        slope_at = F.TOE_RISE_M * (lo[1] - F.TOE_Y) / (constant('BOW_Y') - F.TOE_Y)
        self.assertGreater(lo[2], constant('FLOOR_Z') + slope_at)

    def test_the_arches_leave_the_footwell_open(self):
        inner = min(x for x, _ in F.ARCH_SECTION)
        self.assertGreater(inner, -shell('pedals')[0][0] + 0.2)
        self.assertGreater(inner, shell('seat_commander')[1][0])

    def test_the_arches_stay_inside_the_turned_in_walls(self):
        wall, bow, cab, bow_y = constant('WALL_X'), constant('BOW_X'), constant('CAB_Y'), constant('BOW_Y')
        wall_at_end = wall - (wall - bow) * (F.ARCH_Y[1] - cab) / (bow_y - cab)
        self.assertLess(max(x for x, _ in F.ARCH_SECTION), wall_at_end)

    def test_nobody_walks_on_the_deck_but_through_the_gap(self):
        areas = CABIN['walk']
        aisle = next(a for a in areas if a['to_m'][0] > 0.3 and a['from_m'][1] > 0.9)
        gap = next(a for a in areas if abs(a['from_m'][0]) <= 0.1 and a['to_m'][1] > 3.7)
        self.assertLessEqual(aisle['to_m'][1], F.DECK_AFT_Y)
        self.assertLessEqual(gap['from_m'][1], F.DECK_AFT_Y)


if __name__ == '__main__':
    unittest.main()
