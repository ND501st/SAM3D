"""The PU-12M exterior's trace (tools/blender/pu12m_trace.py) is consistent before anything is modelled on it: the published
dimensions hold, the hull's stations are in order, the wheels fit under and beside the hull, the roof fittings stand on the
roof clear of each other, and the mast nests and reaches its 16 m."""
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / 'blender'))
import pu12m_trace as T  # noqa: E402


def approx(a, b, tol):
    return abs(a - b) <= tol


def hull_half_width_at(x, z):
    """The hull's half-width at (x, z), between the stations: bottom-knee-chine-top piecewise linear."""
    st = T.STATIONS
    for (x0, *a), (x1, *b) in zip(st, st[1:]):
        if x0 <= x <= x1:
            f = (x - x0) / (x1 - x0) if x1 > x0 else 0.0
            pts = [tuple(pa[k] + f * (pb[k] - pa[k]) for k in range(2)) for pa, pb in zip(a, b)]
            if z < pts[0][0] or z > pts[-1][0]:
                return None
            for (z0, w0), (z1, w1) in zip(pts, pts[1:]):
                if z0 <= z <= z1:
                    return w0 + (w1 - w0) * ((z - z0) / (z1 - z0) if z1 > z0 else 1.0)
    return None


def test_published_dimensions():
    assert approx(T.BOW_X - T.STERN_X, T.LENGTH, 0.005)
    widest = max(max(p[1] for p in s[1:]) for s in T.STATIONS)
    assert approx(2 * widest, T.WIDTH, 0.005)
    assert approx(max(s[4][0] for s in T.STATIONS), T.ROOF_Z, 0.001)
    assert approx(min(s[1][0] for s in T.STATIONS), T.CLEARANCE, 0.001)
    assert approx(T.AXLES_X[-1] - T.AXLES_X[0], T.WHEELBASE, 1e-9)
    assert approx(abs(T.WHEELS[0][1] - T.WHEELS[1][1]), T.TRACK, 1e-9)


def test_stations_in_order_and_shaped():
    xs = [s[0] for s in T.STATIONS]
    assert xs == sorted(xs) and xs[0] > T.STERN_X and xs[-1] == T.BOW_X
    for x, (zb, wb), (zk, wk), (zc, wc), (zt, wt) in T.STATIONS:
        assert zb <= zk <= zc <= zt, x
        assert wc >= max(wb, wk, wt) - 1e-9, x  # the chine is the widest line
        assert wc <= T.HALF_W + 1e-9, x


def test_wheels_fit_under_the_chine_and_beside_the_hull():
    r, w = T.TYRE_RADIUS, T.TYRE_WIDTH
    for x, y in T.WHEELS:
        inner = abs(y) - w / 2.0
        outer = abs(y) + w / 2.0
        assert outer <= T.HALF_W, (x, y)  # inside the published width
        # under the chine: the tyre's top is below the chine wherever the tyre is
        for dx in (-r, 0.0, r):
            hw = hull_half_width_at(x + dx, 2 * r * math.cos(math.asin(min(1.0, abs(dx) / r))) if dx else 2 * r)
            if hw is not None:
                assert hw <= T.HALF_W + 1e-9
        # beside the hull: at every height the tyre spans, the hull there is narrower than the tyre's inner face
        for k in range(1, 20):
            z = 2 * r * k / 20.0
            half_chord = math.sqrt(max(0.0, r * r - (z - r) ** 2))
            for dx in (-half_chord, 0.0, half_chord):
                hw = hull_half_width_at(x + dx, z)
                if hw is not None and z < min(s[3][0] for s in T.STATIONS):
                    assert hw < inner, (x, y, z, hw, inner)


def test_wheels_within_the_hull_length():
    for x, _ in T.WHEELS:
        assert T.STERN_X + T.TYRE_RADIUS < x < T.BOW_X - T.TYRE_RADIUS


def roof_items():
    """Footprints on the roof as (name, x0, x1, y0, y1)."""
    items = []
    for i, (x, y, lx, ly) in enumerate(T.ROOF_HATCHES):
        items.append(('hatch%d' % i, x - lx / 2, x + lx / 2, y - ly / 2, y + ly / 2))
    b = T.ROOF_BOX
    items.append(('box', b['x0'], b['x1'], b['y0'], b['y1']))
    r = T.MAST_SECTIONS[0][0]
    items.append(('mast', T.MAST_X - r, T.MAST_X + r, T.MAST_Y - r, T.MAST_Y + r))
    for i, (x, y) in enumerate(T.WHIPS):
        items.append(('whip%d' % i, x - 0.06, x + 0.06, y - 0.06, y + 0.06))
    for i, (x0, x1, y) in enumerate(T.RAILS):
        items.append(('rail%d' % i, x0, x1, y - 0.03, y + 0.03))
    return items


def test_roof_fittings_on_the_roof_and_clear_of_each_other():
    on_roof = [s[0] for s in T.STATIONS if abs(s[4][0] - T.ROOF_Z) < 1e-9]
    roof_x0, roof_x1 = min(on_roof), max(on_roof)  # the roof's rear and front corners
    items = roof_items()
    for name, x0, x1, y0, y1 in items:
        assert roof_x0 - 1e-9 <= x0 and x1 <= roof_x1 + 1e-9, name
        assert -T.ROOF_HALF_W - 1e-9 <= y0 and y1 <= T.ROOF_HALF_W + 1e-9, name
    for i, a in enumerate(items):
        for b in items[i + 1:]:
            overlap = a[1] < b[2] and b[1] < a[2] and a[3] < b[4] and b[3] < a[4]
            assert not overlap, (a[0], b[0])


def test_mast_nests_and_reaches_16_m():
    radii = [r for r, _ in T.MAST_SECTIONS]
    assert radii == sorted(radii, reverse=True)
    stowed = T.mast_section_tops(0.0)
    raised = T.mast_section_tops(1.0)
    assert approx(raised[-1], T.MAST_TOP_RAISED, 1e-9)
    assert approx(max(stowed), T.MAST_STOWED_TOP_Z, 0.06)
    assert stowed[-1] <= T.MAST_STOWED_TOP_Z + 1e-9
    assert raised == sorted(raised)
    # each moving section is long enough to stay inside the one below it when raised (an overlap of 0.2 m at least)
    for i in range(1, len(T.MAST_SECTIONS)):
        bottom = raised[i] - T.MAST_SECTIONS[i][1]
        assert bottom <= raised[i - 1] - 0.2, i
    # half-way it is half-way
    half = T.mast_section_tops(0.5)[-1]
    assert approx(half, 0.5 * (T.MAST_STOWED_TOP_Z + T.MAST_TOP_RAISED), 0.06)


def test_guys_and_the_ab1_stand_clear_of_the_vehicle():
    for k in range(4):
        a = math.pi / 4 + k * math.pi / 2
        sx = T.MAST_X + T.GUY_RADIUS * math.cos(a)
        sy = T.MAST_Y + T.GUY_RADIUS * math.sin(a)
        inside = T.STERN_X <= sx <= T.BOW_X and abs(sy) <= T.HALF_W
        assert not inside, k
    ab = T.AB1
    assert ab['x'] + ab['size'][0] / 2 < T.STERN_X


def test_side_box_stands_on_the_side_clear_of_the_hatches():
    sb = T.SIDE_BOX
    assert 1.375 < sb['z0'] < sb['z1'] < T.ROOF_Z  # on the upper side plate
    for x, zc, lx, lz in T.SIDE_HATCHES:
        overlap = sb['x0'] < x + lx / 2 and x - lx / 2 < sb['x1'] and sb['z0'] < zc + lz / 2 and zc - lz / 2 < sb['z1']
        assert not overlap, x
