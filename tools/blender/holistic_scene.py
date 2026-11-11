"""The built cabin as Unreal places it (USimCabinComponent: the shell parts' meshes, the panels' plates and baked faces,
the blocks, the fixtures), rendered from a reference photo's camera for the holistic layer's look-alike check
(docs/OSA_INTERIOR_HOLISTIC.md section 11). Run by `tools/recon/holistic_view.py render`:

    blender --background --factory-startup --python tools/blender/holistic_scene.py -- <render_job.json>

The job gives the camera (holistic_view.Camera.blender()), the output prefix and the passes:
- `model`: Workbench, studio light, the meshes' colours and the panels' baked faces: what the player would see, flat-lit;
- `id`: every cabin part one flat colour (shell parts by id, panels and their blocks by panel, fixtures by panel), no
  anti-aliasing, and `<prefix>id.json` mapping the colours back, so the comparison knows which part is where.
Blender's world here is the body frame itself (x right, y forward, z up, metres); the FBX meshes, exported in Unreal's
axes (build_cabin_interior.export), are turned a quarter turn into it.
"""
import json
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

LOOK = {   # USimCabinComponent's LookColor (linear)
    'black': (0.015, 0.015, 0.015), 'white': (0.62, 0.6, 0.52), 'grey': (0.22, 0.22, 0.21), 'chrome': (0.6, 0.6, 0.62),
    'brass': (0.55, 0.38, 0.1), 'red': (0.45, 0.02, 0.01), 'green': (0.02, 0.35, 0.04), 'amber': (0.6, 0.28, 0.02),
    'yellow': (0.65, 0.48, 0.12), 'orange': (0.5, 0.14, 0.03), 'bluegrey': (0.16, 0.2, 0.24),
    'lightgrey': (0.36, 0.37, 0.38), 'ivory': (0.7, 0.66, 0.52)}
PLATE = (0.45, 0.47, 0.44)
DARK = (0.03, 0.03, 0.03)
ROUND = {'knob', 'lamp', 'gauge', 'jack', 'socket', 'pointer_knob', 'bezel', 'lamp_button', 'button', 'oval', 'toggle', 'trim'}
FLAT = {'nameplate', 'legend', 'diagram', 'window'}


def log(*a):
    print('holistic_scene:', *a)
    sys.stdout.flush()


def panel_axes(p):
    f, t = math.radians(p['facing_deg']), math.radians(p.get('tilt_deg', 0.0))
    n = Vector((math.sin(f) * math.cos(t), math.cos(f) * math.cos(t), math.sin(t)))
    r = Vector((-math.cos(f), math.sin(f), 0.0))
    return n, r, n.cross(r)


def material(name, rgb, image=None):
    m = bpy.data.materials.new(name)
    m.diffuse_color = (*rgb, 1.0)
    if image:
        m.use_nodes = True
        nt = m.node_tree
        tex = nt.nodes.new('ShaderNodeTexImage')
        tex.image = bpy.data.images.load(image)
        bsdf = nt.nodes.get('Principled BSDF')
        nt.links.new(tex.outputs['Color'], bsdf.inputs['Base Color'])
        nt.nodes.active = tex
    return m


def new_object(name, bm, mat):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    me.materials.append(mat)
    o = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(o)
    return o


def add_box(bm, centre, n, r, u, dn, dr, du):
    """A box centred on `centre`, dn along n, dr along r, du along u."""
    vs = []
    for a in (-0.5, 0.5):
        for b in (-0.5, 0.5):
            for c in (-0.5, 0.5):
                vs.append(bm.verts.new(centre + n * (a * dn) + r * (b * dr) + u * (c * du)))
    idx = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
    for f in idx:
        try:
            bm.faces.new([vs[i] for i in f])
        except ValueError:
            pass


def add_disc(bm, centre, n, r, u, dw, dh, depth, seg=16):
    ring0, ring1 = [], []
    for i in range(seg):
        a = 2 * math.pi * i / seg
        off = r * (0.5 * dw * math.cos(a)) + u * (0.5 * dh * math.sin(a))
        ring0.append(bm.verts.new(centre + off))
        ring1.append(bm.verts.new(centre + off + n * depth))
    bm.faces.new(ring1)
    bm.faces.new(list(reversed(ring0)))
    for i in range(seg):
        j = (i + 1) % seg
        bm.faces.new([ring0[i], ring0[j], ring1[j], ring1[i]])


def add_ring(bm, centre, n, r, u, dw, dh, depth, inner=0.86, seg=32):
    """A bezel: the annulus between `inner` and 1 of the disc, standing `depth` out of the panel (the dial under it is
    printed in the panel's baked face, as Unreal draws it with SM_GaugeBezel)."""
    rings = []
    for k, (scale, dz) in enumerate(((1.0, 0.0), (1.0, depth), (inner, depth), (inner, 0.0))):
        ring = []
        for i in range(seg):
            a = 2 * math.pi * i / seg
            ring.append(bm.verts.new(centre + r * (0.5 * dw * scale * math.cos(a)) + u * (0.5 * dh * scale * math.sin(a))
                                     + n * dz))
        rings.append(ring)
    for k in range(4):
        r0, r1 = rings[k], rings[(k + 1) % 4]
        for i in range(seg):
            j = (i + 1) % seg
            bm.faces.new([r0[i], r0[j], r1[j], r1[i]])


def import_mesh(path, cache):
    if path in cache:
        src = cache[path]
        o = src.copy()
        bpy.context.scene.collection.objects.link(o)
        return o
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=path)
    new = [o for o in bpy.data.objects if o not in before]
    meshes = [o for o in new if o.type == 'MESH']
    for o in new:
        if o.type != 'MESH':
            bpy.data.objects.remove(o, do_unlink=True)
    o = meshes[0]
    o.parent = None
    cache[path] = o
    return o


def build(cab, repo):
    """The cabin's parts, each object tagged with the cabin id it stands for (o['part'])."""
    objs = []
    cache = {}
    art = os.path.join(repo, 'art', 'cabin_interior')
    tex = os.path.join(repo, 'art', 'cabin_textures')
    for s in cab['shell']:
        if s.get('kind') == 'mesh':
            path = os.path.join(art, s['mesh'] + '.fbx')
            if not os.path.exists(path):
                log('no mesh', s['mesh'])
                continue
            o = import_mesh(path, cache)
            o.rotation_mode = 'XYZ'
            o.rotation_euler = (0.0, 0.0, math.radians(90.0 - (s.get('yaw_deg') or 0.0)))
            o.location = Vector(s['centre_m'])
            o.scale = (1.0, 1.0, 1.0)
            o['part'] = s['id']
            objs.append(o)
        elif s.get('kind') == 'box':
            bm = bmesh.new()
            add_box(bm, Vector(s['centre_m']), Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1)), *s['size_m'])
            o = new_object(s['id'], bm, material('m_' + s['id'], LOOK.get(s.get('color'), PLATE)))
            o['part'] = s['id']
            objs.append(o)
    blocks_of = {}
    for b in cab['blocks']:
        blocks_of.setdefault(b['panel'], []).append(b)
    panels = {p['id']: p for p in cab['panels']}
    for p in cab['panels']:
        if p.get('bare'):
            continue
        n, r, u = panel_axes(p)
        c = Vector(p['centre_m'])
        w, h = p['size_m']
        bm = bmesh.new()
        back = 0.01 if p['id'] in blocks_of else 0.0
        add_box(bm, c - n * (0.01 + back), n, r, u, 0.02, w, h)
        for b in blocks_of.get(p['id'], []):
            bc = c + r * b['at_m'][0] + u * b['at_m'][1] - n * 0.005
            add_box(bm, bc, n, r, u, 0.01, b['size_m'][0] - 0.004, b['size_m'][1] - 0.004)
        o = new_object('panel_' + p['id'], bm, material('m_panel_' + p['id'], LOOK.get(p.get('color'), PLATE)))
        o['part'] = p['id']
        objs.append(o)
        img = os.path.join(tex, 'T_Panel_%s.png' % p['id'])
        if os.path.exists(img):
            bm = bmesh.new()
            uv = bm.loops.layers.uv.new('UVMap')
            corners = [(-0.5, -0.5), (0.5, -0.5), (0.5, 0.5), (-0.5, 0.5)]
            vs = [bm.verts.new(c + n * 0.0015 + r * (a * w) + u * (b * h)) for a, b in corners]
            f = bm.faces.new(vs)
            for loop, (a, b) in zip(f.loops, corners):
                loop[uv].uv = (a + 0.5, b + 0.5)
            o = new_object('face_' + p['id'], bm, material('m_face_' + p['id'], PLATE, img))
            o['part'] = p['id']
            objs.append(o)
    by_panel = {}
    for fx in cab['fixtures']:
        by_panel.setdefault(fx['panel'], []).append(fx)
    for pid, fxs in by_panel.items():
        p = panels.get(pid)
        if p is None:
            continue
        n, r, u = panel_axes(p)
        groups = {}
        for fx in fxs:
            groups.setdefault(fx.get('color') or 'none', []).append(fx)
        rings = bmesh.new()     # lamp fixtures' chrome bezels, as Unreal draws them (SM_LampBezel) and the console's own lamps
        for col, items in groups.items():
            bm = bmesh.new()
            for fx in items:
                sz = fx['size_m']
                dw, dh = (sz, sz) if not isinstance(sz, list) else (sz[0], sz[1])
                at = Vector(p['centre_m']) + r * fx['at_m'][0] + u * fx['at_m'][1]
                if fx['kind'] in FLAT:
                    add_box(bm, at + n * 0.001, n, r, u, 0.002, dw, dh)
                elif fx['kind'] == 'gauge' and os.path.exists(os.path.join(tex, 'T_Panel_%s.png' % pid)):
                    add_ring(bm, at, n, r, u, dw, dh, 0.006)
                elif fx['kind'] == 'lamp':
                    add_disc(rings, at, n, r, u, dw, dh, 0.008)
                    add_disc(bm, at + n * 0.008, n, r, u, dw / 1.3, dh / 1.3, 0.006)
                elif fx['kind'] == 'toggle':
                    # The nut and a lever thrown up the face (on), about 15 mm off the plate (g097-a), as Unreal draws a toggle fixture
                    add_disc(bm, at, n, r, u, dw, dh, 0.42 * dw, 6)
                    d = (n * math.cos(math.radians(35.0)) + u * math.sin(math.radians(35.0))).normalized()
                    add_disc(bm, at + n * (0.42 * dw), d, r, d.cross(r).normalized(), 0.4 * dw, 0.4 * dw, 1.2 * dw, 8)
                elif fx['kind'] in ROUND:
                    add_disc(bm, at, n, r, u, dw, dh, 0.012 if fx['kind'] != 'gauge' else 0.006)
                else:
                    add_box(bm, at + n * 0.006, n, r, u, 0.012, dw, dh)
            o = new_object('fix_%s_%s' % (pid, col), bm, material('m_fix_%s_%s' % (pid, col), LOOK.get(col, DARK)))
            o['part'] = 'fix:' + pid
            objs.append(o)
        if rings.verts:
            o = new_object('fix_%s_lamp_rings' % pid, rings, material('m_fix_%s_lamp_rings' % pid, LOOK['chrome']))
            o['part'] = 'fix:' + pid
            objs.append(o)
        else:
            rings.free()
    return objs


def camera(cam):
    sc = bpy.context.scene
    c = bpy.data.objects.new('photo_cam', bpy.data.cameras.new('photo_cam'))
    sc.collection.objects.link(c)
    m = Matrix(cam['matrix']).to_4x4()
    m.translation = Vector(cam['location'])
    c.matrix_world = m
    c.data.sensor_fit = 'HORIZONTAL'
    c.data.sensor_width = cam['sensor_mm']
    c.data.lens = cam['lens_mm']
    c.data.shift_x, c.data.shift_y = cam['shift']
    c.data.clip_start, c.data.clip_end = 0.02, 30.0
    sc.camera = c
    sc.render.resolution_x, sc.render.resolution_y = cam['size']
    sc.render.resolution_percentage = 100


def id_colours(n):
    """n distinct 8-bit sRGB colours, 51 levels apart, black kept for the background."""
    out = []
    for i in range(1, 216):
        rgb = ((i // 36) * 51, ((i // 6) % 6) * 51, (i % 6) * 51)
        if sum(rgb) >= 102:
            out.append(rgb)
    if n > len(out):
        raise RuntimeError('too many parts for the id pass')
    return out[:n]


def srgb_to_linear(c):
    c = c / 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def render(prefix, passes, objs, fill=False):
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_WORKBENCH'
    sc.view_settings.view_transform = 'Standard'
    sc.view_settings.look = 'None'
    sc.view_settings.exposure = 0.0
    sc.view_settings.gamma = 1.0
    sc.render.image_settings.file_format = 'PNG'
    sc.render.dither_intensity = 0.0      # no noise: the id pass is decoded colour by colour, the model's edges by Canny
    sh = sc.display.shading
    if sc.world is None:
        sc.world = bpy.data.worlds.new('w')
    if 'model' in passes:
        sh.light = 'STUDIO'
        sh.color_type = 'TEXTURE'
        sh.show_object_outline = False
        sh.show_cavity = True
        sh.cavity_type = 'WORLD'
        sh.background_type = 'VIEWPORT'
        sh.background_color = (0.0, 0.0, 0.0)
        sc.display.render_aa = '8'
        sc.render.filepath = prefix + 'model.png'
        bpy.ops.render.render(write_still=True)
        log('wrote', sc.render.filepath)
        if fill:                # the studio light leaves a ceiling black: the same view lit flat, for holistic_diff's side-by-side to lift what faces down
            sh.light = 'FLAT'
            sc.render.filepath = prefix + 'model_flat.png'
            bpy.ops.render.render(write_still=True)
            log('wrote', sc.render.filepath)
            sh.light = 'STUDIO'
    if 'id' in passes:
        parts = sorted({o['part'] for o in objs})
        cols = id_colours(len(parts))
        by = dict(zip(parts, cols))
        for o in objs:
            rgb = by[o['part']]
            o.color = (*[srgb_to_linear(v) for v in rgb], 1.0)
        sh.light = 'FLAT'
        sh.color_type = 'OBJECT'
        sh.show_cavity = False
        sh.show_shadows = False
        sh.show_specular_highlight = False
        sh.background_type = 'VIEWPORT'
        sh.background_color = (0.0, 0.0, 0.0)
        sc.display.render_aa = 'OFF'
        sc.render.filepath = prefix + 'id.png'
        bpy.ops.render.render(write_still=True)
        with open(prefix + 'id.json', 'w', encoding='utf-8') as f:
            json.dump({'by_part': {p: list(c) for p, c in by.items()},
                       'ids': {p: {'rgb': list(c)} for p, c in by.items()}}, f, indent=1)
        log('wrote', sc.render.filepath, len(parts), 'parts')


def cast(prefix, rays):
    """Each ray (a detail's pixel seen from the photo's camera) against the built cabin: the first surface it meets,
    which part that is and the surface's normal, so a detail lands on the model's own surface."""
    sc = bpy.context.scene
    dg = bpy.context.evaluated_depsgraph_get()
    out = {}
    for key, o, d in rays:
        hit, loc, nrm, _, obj, _ = sc.ray_cast(dg, Vector(o), Vector(d).normalized(), distance=30.0)
        if hit:
            src = obj.original if obj is not None else None
            out[key] = {'body_m': [round(v, 4) for v in loc], 'normal': [round(v, 3) for v in nrm],
                        'part': src.get('part') if src is not None else None,
                        'range_m': round((loc - Vector(o)).length, 4)}
        else:
            out[key] = None
    with open(prefix + 'cast.json', 'w', encoding='utf-8') as f:
        json.dump(out, f, indent=1)
    log('cast', len(rays), 'rays,', sum(1 for v in out.values() if v), 'hit')


def export_triangles(path, objs):
    """The built cabin as triangles in the body frame with each triangle's part, for the comparison outside Blender (tools/recon/holistic_diff.py)."""
    import array
    dg = bpy.context.evaluated_depsgraph_get()
    parts = sorted({o['part'] for o in objs})
    index = {p: i for i, p in enumerate(parts)}
    tri, ids = array.array('f'), array.array('i')
    for o in objs:
        ev = o.evaluated_get(dg)
        me = ev.to_mesh()
        me.calc_loop_triangles()
        m = ev.matrix_world
        vs = [m @ v.co for v in me.vertices]
        i = index[o['part']]
        for t in me.loop_triangles:
            for q in t.vertices:
                tri.extend(vs[q])
            ids.append(i)
        ev.to_mesh_clear()
    with open(path + '.bin', 'wb') as f:
        tri.tofile(f)
    with open(path + '.part', 'wb') as f:
        ids.tofile(f)
    with open(path, 'w', encoding='utf-8') as f:
        json.dump({'parts': parts, 'triangles': len(ids)}, f)
    log('exported', len(ids), 'triangles,', len(parts), 'parts')


def main():
    job = json.load(open(sys.argv[sys.argv.index('--') + 1], encoding='utf-8'))
    repo = job['repo']
    bpy.ops.wm.read_factory_settings(use_empty=True)
    cab = json.load(open(os.path.join(repo, 'data', 'cabins', 'osa_ak.json'), encoding='utf-8'))
    objs = build(cab, repo)
    log('built', len(objs), 'objects')
    hide = set(job.get('hide', []))           # the vehicle's state in the photo: parts not there as built (folded seats)
    if hide:
        gone = [o for o in objs if o['part'] in hide]
        objs = [o for o in objs if o['part'] not in hide]
        for o in gone:
            bpy.data.objects.remove(o, do_unlink=True)
        log('hid', len(gone), 'objects of', sorted(hide))
    if job.get('export_triangles'):
        export_triangles(job['export_triangles'], objs)
        return
    camera(job['camera'])
    if job.get('rays'):
        cast(job['out'], job['rays'])
    render(job['out'], job['passes'], objs, fill=job.get('fill', False))


if __name__ == '__main__':
    main()
