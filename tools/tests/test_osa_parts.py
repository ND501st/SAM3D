"""The Osa parts rebuilt from the photographs (docs/OSA_MODELLING_PROCESS.md) stay on the photographs' outlines.

Everything is recomputed from what is in the repository: the certified cameras (tools/traces/osa_interior/cameras.json),
the measured outlines (tools/traces/osa_interior/evidence/) and the parts (data/cabins/osa_ak_parts.json). No photograph
and no GPU is needed. A part whose outline error in a view grows past what it was accepted at fails here.
"""
import os
import sys

import pytest
np = pytest.importorskip("numpy")  # the tool's own dependencies, as test_photo_trace.py
pytest.importorskip("cv2")

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO, 'tools', 'recon', 'osa'))
import core as C  # noqa: E402
import evidence as E  # noqa: E402
import parts as P  # noqa: E402
import store  # noqa: E402

GROW = 1.2          # an accepted error may grow by this much, or by half a pixel, before the test fails
PARTS = store.parts()
CASES = [(pid, view) for pid, p in PARTS.items() for view in p.get('accepted', {})]


def dense(X, step=0.004):
    out = [X[:1]]
    for a, b in zip(X[:-1], X[1:]):
        n = max(1, int(np.linalg.norm(b - a) / step))
        out.append(a + (b - a) * np.linspace(0, 1, n + 1)[1:, None])
    return np.vstack(out)


def error(pid, view):
    p = PARTS[pid]
    shape = store.shape(p)
    cam = C.camera(view, 'A')
    e = []
    for feat in p['evidence_features']:
        for it in E.items(view, pid, feat):
            if feat in p.get('feature_lines_m', {}):
                X, closed = np.array(p['feature_lines_m'][feat], float), False
            else:
                X, closed = shape.features()[p['evidence_features'][feat]]
            if p['evidence_features'][feat] == 'hull':
                e.append(C.outline_error(cam, X, it['pieces'], hull=True))
            else:
                e.append(C.outline_error(cam, dense(np.vstack([X, X[:1]]) if closed else X), it['pieces']))
    e = np.concatenate(e)
    return float(np.median(e)), float(np.percentile(e, 90))


def test_there_are_parts_and_cameras():
    assert PARTS, 'no rebuilt parts'
    cams = C.cameras()
    assert sum(1 for v in cams.values() if v['grade'] == 'A') >= 10


def test_grade_a_cameras_hold_their_control():
    doc = C.load(C.CAMERAS)
    lim = doc['limits']['A_control_px']
    for name, v in doc['views'].items():
        if v['grade'] == 'A':
            assert v['control'], name + ' is grade A without control'
            assert max(c['median_px'] for c in v['control'].values()) <= lim, name
            assert all(c['matches'] >= 60 for c in v['control'].values()), name


def test_a_plate_is_never_the_frame():
    ctl = C.load(C.CONTROL)
    frames = [f for f in ctl['faces'] if f['kind'] == 'frame']
    assert [f['id'] for f in frames] == ['sp2m']
    doc = C.load(C.CAMERAS)
    assert doc['faces']['sp2m']['moved_m'] == [0.0, 0.0, 0.0] and doc['faces']['sp2m']['scale'] == 1.0


@pytest.mark.parametrize('pid', sorted(PARTS))
def test_part_is_sourced_and_says_what_was_chosen(pid):
    p = PARTS[pid]
    assert 'wwp_gecko' in p['source'] and p['confidence'] in ('high', 'medium', 'low')
    assert p['measured'] and p['chosen']


@pytest.mark.parametrize('pid', sorted(PARTS))
def test_part_mesh_is_a_closed_shell(pid):
    V, F = store.shape(PARTS[pid]).mesh()
    edges = {}
    for tri in F:
        for a, b in ((tri[0], tri[1]), (tri[1], tri[2]), (tri[2], tri[0])):
            edges[(min(a, b), max(a, b))] = edges.get((min(a, b), max(a, b)), 0) + 1
    assert np.isfinite(V).all()
    assert all(n == 2 for n in edges.values()), '%s: %d edges are not shared by two faces' % (pid, sum(1 for n in edges.values() if n != 2))


@pytest.mark.parametrize('pid,view', CASES)
def test_outline_error_has_not_grown(pid, view):
    was = PARTS[pid]['accepted'][view]
    med, p90 = error(pid, view)
    assert med <= max(GROW * was['median_px'], was['median_px'] + 0.5), '%s in %s: %.1f px, accepted at %.1f' % (pid, view, med, was['median_px'])
    assert p90 <= max(GROW * was['p90_px'], was['p90_px'] + 1.0), '%s in %s: worst tenth %.1f px, accepted at %.1f' % (pid, view, p90, was['p90_px'])


def test_the_post_and_its_housing_pass_in_three_views():
    for pid in ('post.right', 'housing.right'):
        ok = [v for v, a in PARTS[pid]['accepted'].items() if a['passes'] and a['how'] == 'evidence']
        assert len(ok) >= 3, '%s passes in %s' % (pid, ok)


def test_ear_clip_covers_the_polygon():
    poly = np.array([[0, 0], [2, 0], [2, 1], [1, 1], [1, 2], [0, 2]], float)      # an L: area 3
    tris = P.ear_clip(poly)
    area = sum(0.5 * abs((poly[b] - poly[a])[0] * (poly[c] - poly[a])[1] - (poly[b] - poly[a])[1] * (poly[c] - poly[a])[0]) for a, b, c in tris)
    assert abs(area - 3.0) < 1e-9 and len(tris) == 4
