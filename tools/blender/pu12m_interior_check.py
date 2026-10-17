"""Find parts of the PU-12M's compartment that run into each other (docs/PU12_DEVELOPMENT.md P6, the modelling doc's
"overlap tests" taken to the built meshes).

The trace's tests (tools/tests/test_pu12m_interior_trace.py) check the items' boxes. This builds what Unreal shows:
- every equipment item as its own mesh (build_pu12m_interior.build_item);
- the bench, the front seats where the layout puts them, the driver's parts, the mast's foot;
- the reused meshes (the R-123Ms) as boxes of their size, turned by their yaw;
- the layout's runtime parts as boxes: each panel's plate (2 cm behind its plane), each control, lamp and fixture (in
  front of it), each screen.

It then tests every pair for triangles that intersect (BVH overlap) and reports them, with how deep they go. Expected
contacts are not reported: a runtime part on its own item, an item on the floor or against the lining.

    "C:/Program Files/Blender Foundation/Blender 4.2/blender.exe" --background --python tools/blender/pu12m_interior_check.py
    ... -- --json out/pu12m_interior/check.json
    ... -- --poses out/crew_poses_pu12m.json      # also the crew's bodies (crew_pose_dump data <file> pu12m)

Prints one line per pair, the worst first, and the count; exits 0 (it reports, the tests judge).
"""
import json
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_cabin_interior as C  # noqa: E402
import build_pu12m_interior as B  # noqa: E402
import pu12m_interior_trace as I  # noqa: E402

REPO = C.REPO
LAYOUT = os.path.join(REPO, 'data', 'cabins', 'pu12m.json')
TOUCH = 0.002   # contacts shallower than this are touching, not running into each other
LINING_MOUNT = 0.01  # a block mounted flush on the sloping lining may sink this far into it unseen
BODY_SLACK = 0.02    # the bodies are boxes round rounded limbs: shallower contacts are their corners


class Part:
    def __init__(self, name, kind, verts, tris, host=None):
        self.name, self.kind, self.host = name, kind, host
        self.verts = verts          # Blender coordinates
        self.tris = tris
        self.lo = Vector((min(v.x for v in verts), min(v.y for v in verts), min(v.z for v in verts)))
        self.hi = Vector((max(v.x for v in verts), max(v.y for v in verts), max(v.z for v in verts)))
        self.bvh = BVHTree.FromPolygons(verts, tris)


def from_builder(name, kind, fill, host=None):
    b = C.Builder(name)
    fill(b)
    bm = b.bm
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    verts = [v.co.copy() for v in bm.verts]
    tris = [[v.index for v in f.verts] for f in bm.faces]
    bm.free()
    return Part(name, kind, verts, tris, host)


def from_object(name, kind, obj, matrix=None, host=None):
    bpy.context.view_layer.update()  # the object's placement (finish sets its location) into matrix_world
    m = (matrix or Matrix.Identity(4)) @ obj.matrix_world
    mesh = obj.data
    mesh.calc_loop_triangles()
    verts = [m @ v.co for v in mesh.vertices]
    tris = [list(t.vertices) for t in mesh.loop_triangles]
    return Part(name, kind, verts, tris, host)


def box_part(name, kind, centre, axes, half, host=None):
    """An oriented box (body frame centre and three unit axes, half sizes along them)."""
    c = Vector(centre)
    corners = []
    for sx in (-1, 1):
        for sy in (-1, 1):
            for sz in (-1, 1):
                p = c + Vector(axes[0]) * (sx * half[0]) + Vector(axes[1]) * (sy * half[1]) + Vector(axes[2]) * (sz * half[2])
                corners.append(C.P(p.x, p.y, p.z))
    idx = lambda sx, sy, sz: (sx * 4) + (sy * 2) + sz  # noqa: E731
    quads = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
    tris = []
    for q in quads:
        tris += [[q[0], q[1], q[2]], [q[0], q[2], q[3]]]
    del idx
    return Part(name, kind, corners, tris, host)


def panel_axes(p):
    f, t = math.radians(p.get('facing_deg', 0.0)), math.radians(p.get('tilt_deg', 0.0))
    n = Vector((math.sin(f) * math.cos(t), math.cos(f) * math.cos(t), math.sin(t)))
    r = Vector((-math.cos(f), math.sin(f), 0.0))
    u = n.cross(r)
    return n, r, u


def host_item(centre_body):
    """The trace item whose face a panel lies on: the nearest box (trace X = body y, Y = -body x)."""
    x, y, z = centre_body[1], -centre_body[0], centre_body[2]
    best, dist = None, 1e9
    for it in I.ITEMS:
        dx = max(it['x0'] - x, 0.0, x - it['x1'])
        dy = max(it['y0'] - y, 0.0, y - it['y1'])
        dz = max(it['z0'] - z, 0.0, z - it['z1'])
        d = math.sqrt(dx * dx + dy * dy + dz * dz)
        if d < dist:
            best, dist = it['id'], d
    return best if dist < 0.03 else None


def layout_parts(layout):
    out = []
    panels = {p['id']: p for p in layout['panels']}
    hosts = {}
    for p in layout['panels']:
        n, r, u = panel_axes(p)
        c = Vector(p['centre_m'])
        hosts[p['id']] = host_item(p['centre_m'])
        if p.get('bare'):
            continue  # the front plate of a mesh: nothing drawn
        w, h = p['size_m']
        out.append(box_part('panel ' + p['id'], 'panel', c - n * 0.01, (n, r, u), (0.01, w / 2, h / 2), host=hosts[p['id']]))
    for kind, items in (('control', layout['controls']), ('lamp', layout['lamps']), ('fixture', layout['fixtures'])):
        for k, c in enumerate(items):
            p = panels[c['panel']]
            n, r, u = panel_axes(p)
            look = c.get('look', {}) if kind != 'fixture' else c
            size = look.get('size_m', 0.02 if kind != 'lamp' else 0.016)
            w, h = (size, size) if not isinstance(size, list) else size
            depth = {'toggle': 0.03, 'knob': 0.025, 'wheel': 0.04, 'button': 0.012, 'lamp_button': 0.015, 'nameplate': 0.002,
                     'window': 0.002, 'gauge': 0.01}.get(look.get('kind', c.get('style', '')), 0.015)
            u_m, v_m = c['at_m']
            at = Vector(p['centre_m']) + r * u_m + u * v_m
            name = '%s %s' % (kind, c.get('control') or c.get('id') or '%s#%d' % (c['panel'], k))
            out.append(box_part(name, kind, at + n * (depth / 2 + 0.0005), (n, r, u), (depth / 2, w / 2, h / 2),
                                host=hosts[c['panel']]))
    for d in layout['displays']:
        p = panels[d['panel']]
        n, r, u = panel_axes(p)
        u_m, v_m = d['at_m']
        at = Vector(p['centre_m']) + r * u_m + u * v_m
        w, h = d['size_m']
        out.append(box_part('display ' + d['id'], 'display', at + n * 0.007, (n, r, u), (0.007, w / 2 + 0.006, h / 2 + 0.006),
                            host=hosts[d['panel']]))
    return out, hosts


def mesh_parts(layout):
    out = []
    lining = B.build_lining()
    out.append(from_object('lining', 'lining', lining))
    for it in I.ITEMS:
        if it['id'] in B.EQUIPMENT_SKIP:
            continue
        out.append(from_builder('item ' + it['id'], 'item', lambda b, it=it: B.build_item(b, it), host=it['id']))
    for name, fn, host in (('bench', B.build_bench, 'bench'), ('steering', B.build_steering, None),
                           ('pedals', B.build_pedals, None), ('driver', B.build_driver, 'dash'),
                           ('mastfoot', B.build_mast_foot, 'mast')):
        out.append(from_object(name, 'part', fn(), host=host))
    seat = B.build_front_seat()
    bpy.context.view_layer.update()  # its own placement into matrix_world before the copies are placed from it
    for s in layout['shell']:
        if s.get('kind') != 'mesh':
            continue
        if s['mesh'] == 'SM_PU12MFrontSeat':
            cx, cy, cz = s['centre_m']
            m = Matrix.Translation(C.P(cx, cy, cz) - seat.matrix_world.translation)
            out.append(from_object(s['id'], 'part', seat, matrix=m, host=s['id']))
        elif s['mesh'] in ('SM_RadioR123M',):
            yaw = math.radians(s.get('yaw_deg', 0.0))
            ax = (Vector((math.cos(yaw), -math.sin(yaw), 0)), Vector((math.sin(yaw), math.cos(yaw), 0)), Vector((0, 0, 1)))
            sx, sy, sz = s['size_m']
            out.append(box_part(s['id'], 'part', s['centre_m'], ax, (sx / 2, sy / 2, sz / 2), host=s['id']))
    return out


# The crewman's parts as boxes (half width across, half depth; build_crew_body.py's sizes, rounded up a little).
BODY_HALF = {'pelvis': (0.16, 0.10), 'torso': (0.17, 0.10), 'neck': (0.05, 0.05), 'head': (0.09, 0.11),
             'upper_arm': (0.045, 0.045), 'forearm': (0.04, 0.04), 'hand': (0.045, 0.02), 'thigh': (0.07, 0.07),
             'shin': (0.05, 0.05), 'foot': (0.045, 0.035)}
SEATS = {'bench', 'seat_driver', 'seat_squad_leader'}


def body_parts(pose, layout, hosts):
    """One pose's parts, each knowing what it may touch: its seat, the floor (feet), the control it reaches and that
    control's block (hand, forearm), the wheel and the pedals (the driver)."""
    touched = None
    if pose.get('control'):
        for c in layout['controls']:
            if c['control'] == pose['control']:
                touched = hosts.get(c['panel'])
    out = []
    for part in pose['parts']:
        o, bone, up = Vector(part['origin']), Vector(part['bone']), Vector(part['up'])
        length = part['length']                       # the bone is a direction
        if length < 1e-4 or bone.length < 1e-6:
            continue
        along = bone.normalized()
        bone = along * length
        upn = (up - along * up.dot(along)).normalized()
        side = along.cross(upn)
        kind = part['name'].rsplit('_', 1)[0] if part['name'][-2:] in ('_l', '_r') else part['name']
        hw, hd = BODY_HALF.get(kind, (0.04, 0.04))
        bp = box_part('body %s %s' % (pose['name'], part['name']), 'body', o + bone * 0.5, (along, side, upn),
                      (length / 2, hw, hd))
        bp.may_touch = set(SEATS)
        if kind in ('foot', 'shin'):
            bp.may_touch |= {'lining', 'pedals'}
        if kind in ('hand', 'forearm'):
            bp.may_touch |= {'steering'}
            if touched:
                bp.may_touch.add('host:' + touched)
        out.append(bp)
    return out


def body_expected(body, other):
    if other.name in body.may_touch:
        return True
    if other.host is not None and ('host:' + other.host) in body.may_touch:
        return True
    return False


def expected(a, b, hosts_of_runtime):
    """Contacts by design."""
    kinds = {a.kind, b.kind}
    runtime = {'panel', 'control', 'lamp', 'fixture', 'display'}
    for x, y in ((a, b), (b, a)):
        # a runtime part on its own item, or on the mesh standing for it (an R-123M's panel is bare)
        if x.kind in runtime and y.host is not None and x.host == y.host:
            return True
        # runtime parts of one panel touch each other's footprint (the plate under the controls)
        if x.kind == 'panel' and y.kind in runtime and x.host == y.host:
            return True
    if a.kind in runtime and b.kind in runtime and a.host == b.host:
        return True
    # the mast goes up through the roof; the wheel's hub sits on its column
    if {a.name, b.name} in ({'lining', 'mastfoot'}, {'steering', 'driver'}):
        return True
    # the bench's frame stands on the floor plates and against the lining's floor
    del kinds
    return False


def inside(part, p):
    """Is p inside the part's solid? (A ray to +X crosses its surface an odd number of times; two skewed rays agree.)"""
    votes = 0
    for d in (Vector((1.0, 0.0137, 0.0071)).normalized(), Vector((-0.0113, 0.0091, 1.0)).normalized()):
        n, o = 0, p.copy()
        for _ in range(64):
            hit, _nrm, _i, dist = part.bvh.ray_cast(o, d)
            if hit is None:
                break
            n += 1
            o = hit + d * 1e-5
        votes += n % 2
    return votes == 2


def samples(part):
    """The part's vertices and points along its edges every 2 cm (a leg through a drum has no vertex inside it)."""
    pts = list(part.verts)
    seen = set()
    for t in part.tris:
        for i in range(3):
            e = (min(t[i], t[(i + 1) % 3]), max(t[i], t[(i + 1) % 3]))
            if e in seen:
                continue
            seen.add(e)
            p0, p1 = part.verts[e[0]], part.verts[e[1]]
            n = int((p1 - p0).length / 0.02)
            pts += [p0.lerp(p1, (k + 1) / (n + 1)) for k in range(n)]
    return pts


def penetration(a, b):
    """How deep a's points (vertices and along its edges) go into b's solid (0 when none is inside), and where (body
    frame)."""
    worst, where = 0.0, None
    if not hasattr(a, 'pts'):
        a.pts = samples(a)
    for v in a.pts:
        if not all(b.lo[k] - 1e-6 <= v[k] <= b.hi[k] + 1e-6 for k in range(3)):
            continue
        if not inside(b, v):
            continue
        _loc, _n, _i, dist = b.bvh.find_nearest(v)
        if dist is not None and dist > worst:
            worst, where = dist, (-v.y, v.x, v.z)   # Blender (X forward, Y left) to the body frame
    return worst, where


def depth(a, b):
    return max(penetration(a, b), penetration(b, a), key=lambda r: r[0])


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    bpy.ops.wm.read_factory_settings(use_empty=True)
    coll = bpy.data.collections.new(C.COLLECTION)
    bpy.context.scene.collection.children.link(coll)
    with open(LAYOUT, encoding='utf-8') as f:
        layout = json.load(f)
    runtime, hosts = layout_parts(layout)
    parts = mesh_parts(layout) + runtime
    found = []
    if '--poses' in argv:
        with open(os.path.abspath(argv[argv.index('--poses') + 1]), encoding='utf-8') as f:
            poses = json.load(f)['poses']
        for pose in poses:
            for bp in body_parts(pose, layout, hosts):
                for other in parts:
                    if any(bp.lo[k] > other.hi[k] - TOUCH or other.lo[k] > bp.hi[k] - TOUCH for k in range(3)):
                        continue
                    if body_expected(bp, other) or not bp.bvh.overlap(other.bvh):
                        continue
                    d, where = depth(bp, other)
                    if d >= BODY_SLACK:
                        found.append((d, bp.name, other.name, 1, where))
    for i, a in enumerate(parts):
        for b in parts[i + 1:]:
            if any(a.lo[k] > b.hi[k] - TOUCH or b.lo[k] > a.hi[k] - TOUCH for k in range(3)):
                continue
            if expected(a, b, hosts):
                continue
            if 'lining' in (a.name, b.name) and {a.kind, b.kind} & {'item', 'part'}:
                # against the lining: only what goes through the wall counts (a box standing on the floor touches it)
                pass
            pairs = a.bvh.overlap(b.bvh)
            if not pairs:
                continue
            d, where = depth(a, b)
            if d < (LINING_MOUNT if 'lining' in (a.name, b.name) else TOUCH):
                continue
            found.append((d, a.name, b.name, len(pairs), where))
    found.sort(key=lambda f: -f[0])
    for d, a, b, n, w in found:
        print('INTERSECT %-30s %-30s depth %5.1f cm at (%.2f, %.2f, %.2f)  (%d triangle pairs)' % (a, b, d * 100.0, *w, n))
    print('intersecting pairs:', len(found))
    if '--json' in argv:
        path = os.path.abspath(argv[argv.index('--json') + 1])
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'w', encoding='utf-8') as f:
            json.dump([{'a': a, 'b': b, 'depth_m': round(d, 4), 'at_body_m': [round(c, 3) for c in w], 'triangles': n}
                       for d, a, b, n, w in found], f, indent=1)


if __name__ == '__main__':
    main()
