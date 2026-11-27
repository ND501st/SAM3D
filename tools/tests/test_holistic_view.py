"""The holistic layer's look-alike check (docs/OSA_INTERIOR_HOLISTIC.md sections 11-13): the camera, the posing, the
casting of details and the files' checks. Synthetic cameras with known answers; no Blender."""
import math
import os
import sys

import pytest
np = pytest.importorskip("numpy")  # the tool's own dependencies, as test_photo_trace.py

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'recon'))
import holistic as H  # noqa: E402
import holistic_view as V  # noqa: E402


def _look_at(eye, target, w=1600, h=1200, f=1400.0):
    eye, target = np.array(eye, float), np.array(target, float)
    z = target - eye
    z /= np.linalg.norm(z)
    x = np.cross(z, [0.0, 0.0, 1.0])
    x /= np.linalg.norm(x)
    y = np.cross(z, x)
    R = np.stack([x, y, z])
    return V.Camera(f, w / 2, h / 2, R, -R @ eye, w, h)


def test_camera_projects_and_casts_back():
    cam = _look_at([0.0, 3.0, 1.6], [1.0, 1.5, 1.4])
    X = np.array([[0.9, 1.2, 1.1], [0.6, 2.4, 1.9]])
    p = cam.project(X)
    for x, q in zip(X, p):
        o, d = cam.ray(q)
        t = np.dot(x - o, d)
        assert np.linalg.norm(o + t * d - x) < 1e-9
    assert np.isnan(cam.project([[0.0, 4.0, 1.6]])).all()        # behind the camera
    assert np.allclose(cam.centre(), [0.0, 3.0, 1.6])


def test_blender_camera_looks_where_the_camera_looks():
    cam = _look_at([0.0, 3.0, 1.6], [1.0, 1.5, 1.4])
    b = cam.blender()
    m = np.array(b['matrix'])
    assert np.allclose(m.T @ m, np.eye(3), atol=1e-9)
    assert np.allclose(-m[:, 2], cam.R[2])                       # Blender looks down its -z
    assert np.allclose(m[:, 1], -cam.R[1])                       # with +y up
    assert abs(b['lens_mm'] - cam.f * 36.0 / cam.w) < 1e-9 and b['shift'] == [0.0, 0.0]


def test_anchors_recover_the_camera():
    true = _look_at([-0.3, 3.1, 1.55], [0.8, 1.2, 1.3], f=1500.0)
    rng = np.random.default_rng(3)
    P3 = np.column_stack([rng.uniform(0.4, 1.2, 12), rng.uniform(0.3, 2.6, 12), rng.uniform(0.7, 2.1, 12)])
    p2 = true.project(P3) + rng.normal(0.0, 0.5, (12, 2))
    cam, res = V.solve_anchors(P3, p2, true.w, true.h)
    assert abs(cam.f - true.f) / true.f < 0.02, cam.f
    assert np.linalg.norm(cam.centre() - true.centre()) < 0.02
    assert float(np.sqrt(np.mean(res ** 2))) < 1.5


def test_eye_box_keeps_the_photographer_in_the_compartment():
    # anchors on one plane (a console's face) leave f and the distance trading off; the box says where a person can
    # stand, and the solve must then return an eye inside it at the true camera
    assert V._outside([0.0, 1.0, 1.5], V.EYE_BOX) == 0.0
    assert abs(V._outside([0.75, 1.0, 1.5], V.EYE_BOX) - 0.30) < 1e-9
    true = _look_at([0.3, 1.9, 1.75], [-0.734, 1.3, 1.55], w=2000, h=1500, f=1100.0)
    rng = np.random.default_rng(5)
    P3 = np.column_stack([np.full(14, -0.734), rng.uniform(0.6, 2.5, 14), rng.uniform(1.1, 2.1, 14)])
    p2 = true.project(P3) + rng.normal(0.0, 0.3, (14, 2))
    cam, res = V.solve_anchors(P3, p2, true.w, true.h, eye_box=V.EYE_BOX)
    assert V._outside(cam.centre(), V.EYE_BOX) == 0.0
    assert np.linalg.norm(cam.centre() - true.centre()) < 0.1, cam.centre()
    assert float(np.sqrt(np.mean(res ** 2))) < 1.0


def test_view_checks_pending_state_and_checks():
    cab, reg = V.load_cabin(), V.load_register()
    pending = {'photo': 't', 'image': 'clean', 'camera_from': 'pending'}
    assert any('why' in e for e in V.check_view(pending, cab, reg))
    assert V.check_view(dict(pending, why='nothing to anchor on'), cab, reg) == []
    a = [{'at': 'panel:so_panel@c', 'px': [1.0, 2.0]}] * 6
    view = {'photo': 't', 'image': 'clean', 'camera_from': 'anchors', 'anchors': a}
    assert V.check_view(view, cab, reg) == []
    unwhy = dict(view, anchors=a + [{'at': 'panel:fo_panel@c', 'px': [1.0, 2.0], 'use': False}])
    assert any('check' in e and 'why' in e for e in V.check_view(unwhy, cab, reg))
    too_few = dict(view, anchors=a[:5] + [{'at': 'panel:fo_panel@c', 'px': [1.0, 2.0], 'use': False, 'why': 'x'}])
    assert any('six anchors' in e for e in V.check_view(too_few, cab, reg))
    st = dict(view, state={'hide': ['seat_deputy', 'no_such_part']})
    errs = V.check_view(st, cab, reg)
    assert any('its why' in e for e in errs) and any('no_such_part' in e for e in errs)
    assert any('eye_box' in e for e in V.check_view(dict(view, eye_box=[[0.5, 0.1], [0, 1], [0, 1]]), cab, reg))


def test_face_camera_moves_to_the_body_frame():
    # a camera solved on SP-2M2's face (face frame: x right, y down, z into it, origin top left) must see the panel's
    # corners where it saw the face's corners
    cab = V.load_cabin()
    p = cab['panels']['sp2m_panel']
    w, h = p['size_m']
    a = math.radians(25.0)        # seen from the aisle, a little from one side, the whole face in front of it
    Rf = np.array([[math.cos(a), 0.0, -math.sin(a)], [0.0, 1.0, 0.0], [math.sin(a), 0.0, math.cos(a)]])
    tf = np.array([-0.95, -0.62, 2.0])
    face = {'R': Rf.tolist(), 't': tf.tolist(), 'f_px': 2000.0, 'w': 2000, 'h': 1800}
    cam = V.face_camera(face, p, w, h)
    for (fx, fy), corner in (((0, 0), 'tl'), ((w, 0), 'tr'), ((w, h), 'br'), ((0, h), 'bl')):
        xc = Rf @ np.array([fx, fy, 0.0]) + tf
        want = [2000.0 * xc[0] / xc[2] + 1000.0, 2000.0 * xc[1] / xc[2] + 900.0]
        got = cam.project(V.resolve('panel:sp2m_panel@' + corner, cab, {}))[0]
        assert np.allclose(got, want, atol=1e-6), (corner, got, want)


def test_ray_meets_a_box_from_outside_and_inside():
    hit = V.ray_box([0.0, 0.0, 0.0], [1.0, 0.0, 0.0], [2.0, 0.0, 0.0], [1.0, 1.0, 1.0])
    assert np.allclose(hit, [1.5, 0.0, 0.0])
    inside = V.ray_box([0.0, 0.0, 0.0], [0.0, 0.0, 1.0], [0.0, 0.0, 0.0], [2.0, 2.0, 3.0])
    assert np.allclose(inside, [0.0, 0.0, 1.5])                   # a roof seen from within
    assert V.ray_box([0.0, 0.0, 0.0], [-1.0, 0.0, 0.0], [2.0, 0.0, 0.0], [1.0, 1.0, 1.0]) is None
    turned = V.ray_box([0.0, -2.0, 0.5], [0.0, 1.0, 0.0], [0.0, 0.0, 0.5], [0.2, 1.0, 1.0], yaw_deg=90.0)
    assert np.allclose(turned, [0.0, -0.1, 0.5])                  # a part turned a quarter: its long side across


def test_points_on_the_cabin():
    cab, reg = V.load_cabin(), V.load_register()
    p = cab['panels']['sp2m_panel']
    tl = V.resolve('panel:sp2m_panel@tl', cab, reg)
    assert abs(tl[2] - (p['centre_m'][2] + p['size_m'][1] / 2)) < 1e-9
    assert tl[1] > p['centre_m'][1]                               # SP-2M2 faces the aisle: its left is forward
    s = cab['shell']['radio_left']
    top = V.resolve('shell:radio_left@00+', cab, reg)
    assert abs(top[2] - (s['centre_m'][2] + s['size_m'][2] / 2)) < 1e-9


def test_build_rule():
    assert V.build_rule('fastener', 3.0) == 'mesh'
    assert V.build_rule('seam', 0.8) == 'normal'
    assert V.build_rule('label', 0.0) == 'decal'
    assert V.build_rule('surface', 0.0) == 'texture'
    assert V.build_rule('profile') == 'mesh' and V.build_rule('fastener', 0.1, silhouette=True) == 'mesh'
    assert set(k for _, k in V.DETAIL_KINDS.values()) <= set(V.BUILD)


def test_detail_checks():
    reg = V.load_register()
    good = {'photo': 't', 'details': [{'id': 'd1', 'on': 'rack.sp2m', 'kind': 'fastener', 'what': 'screws',
                                       'point': [0.5, 0.5], 'relief': 'raised', 'build': 'mesh', 'confidence': 'high',
                                       'status': 'absent'}]}
    assert V.check_details(good, reg) == []
    bad = dict(good, details=[dict(good['details'][0], kind='bolt', point=[1.5, 0.5], status='not_built')])
    errs = V.check_details(bad, reg)
    assert any('kind' in e for e in errs) and any('fractions' in e for e in errs) and any('why' in e for e in errs)


def test_ids_as_the_register_keeps_them():
    reg = V.load_register()
    assert V.canon('new.frame.post_sp2m_sp3m', reg) == 'frame.post_sp2m_sp3m'    # an accepted proposal
    assert V.canon('new.kit.t_key_rear_right', reg) == 'kit.keys_post_right'       # H3's alias
    assert V.canon('new.unknown.nowhere', reg) == 'new.unknown.nowhere'


def test_a_traced_face_belongs_to_its_rack():
    owns = V.ownership(V.load_register(), V.load_cabin())
    assert 'sp2m_face' in owns['rack.sp2m']                    # the bare panel's mesh
    assert 'sp2m_face' in owns['block.sp2_m1']                 # a block on it


def test_view_and_detail_files_are_valid():
    cab, reg = V.load_cabin(), V.load_register()
    vs = V.views()
    assert vs, 'at least the pilot view (g092-a)'
    for v in vs:
        assert V.check_view(v, cab, reg) == [], v['photo']
    for d in V.detail_files():
        assert V.check_details(d, reg) == [], d['photo']
        assert {x['kind'] for x in d['details']} <= set(V.DETAIL_KINDS)


def test_every_wide_photo_has_a_view_and_an_inventory():
    # the roadmap's order: H6 and H8 on every wide photo before I1; a view may be pending, but only with its reason
    wide = {'g082-b', 'g089-d', 'g092-a', 'g094-a', 'g096-a', 'g100-a'}
    views = {v['photo']: v for v in V.views()}
    assert wide <= set(views)
    assert all(views[p]['camera_from'] != 'pending' or views[p].get('why') for p in wide)
    inv = {d['photo']: d['details'] for d in V.detail_files()}
    assert wide <= set(inv)
    for p in wide:                                   # structure as well as hardware, in every one
        kinds = {x['kind'] for x in inv[p]}
        assert 'profile' in kinds and kinds & {'fastener', 'clip', 'label'}, p


def test_the_pilot_inventory_covers_structure_and_small_things():
    # the first inventory must not be only the big things: profiles, fasteners, labels and clips are what a model lacks
    d = [x for x in V.detail_files() if x['photo'] == 'g092-a'][0]['details']
    kinds = {x['kind'] for x in d}
    for k in ('profile', 'fastener', 'label', 'clip', 'surface', 'rib', 'edge'):
        assert k in kinds, k
    assert all(math.isfinite(p) for x in d for q in V._detail_points(x) for p in q)
    assert H.CONFIDENCE == ['high', 'medium', 'low']


def test_a_view_from_the_joint_solve_needs_its_why():
    v = {'photo': 'g098-a', 'image': 'clean', 'camera_from': 'sfm'}
    assert V.check_view(v, {'panels': {}, 'shell': {}}, {}) == ['g098-a: an sfm camera needs its why']
    v['why'] = 'the joint solve of the photos'
    assert V.check_view(v, {'panels': {}, 'shell': {}}, {}) == []
