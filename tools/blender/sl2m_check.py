"""SŁ-2M as the game draws it, in Blender, to check against SAM Simulator's AK photo (docs/OSA_INTERIOR_HOLISTIC.md 54).

Builds the rack SM_RackSL2M as build_cabin_interior.py does (its housings and mounts), lays the baked panel faces
art/cabin_textures/T_Panel_<panel>.png on it, and places every control, lamp, display and fixture of the three columns
from data/cabins/osa_ak.json with the parts library (art/cabin_parts/cabin_parts.blend) at the sizes, colours and
offsets SimCabinComponent uses. Each column is rendered square-on in orthographic at the scale of its fit to the photo
(the `--fits` JSON: u = a + s * x, v = b - s * y in the photo's pixels), so the render lies pixel for pixel on the photo;
the window it covers goes to <out>/<panel>.json for the overlay (tools/sl2m_overlay.py).

    blender --background --factory-startup --python tools/blender/sl2m_check.py -- --fits FITS.json --out DIR [--blend]
"""
import json
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(REPO, 'tools'))
import build_cabin_interior as B  # noqa: E402
import panel_sheet  # noqa: E402

PANELS = ('op_panel', 'so_panel', 'fo_panel')
PARTS = os.path.join(REPO, 'art', 'cabin_parts', 'cabin_parts.blend')

# SimCabinComponent's look colours (LookColor) and lamp colours, linear
LOOK = {'black': (0.02, 0.02, 0.02), 'white': (0.8, 0.8, 0.78), 'grey': (0.25, 0.25, 0.25), 'chrome': (0.6, 0.6, 0.62),
        'brass': (0.55, 0.4, 0.15), 'red': (0.5, 0.02, 0.02), 'green': (0.05, 0.3, 0.05), 'amber': (0.6, 0.3, 0.02),
        'yellow': (0.7, 0.6, 0.05), 'orange': (0.6, 0.2, 0.02), 'bluegrey': (0.2, 0.25, 0.3), 'lightgrey': (0.5, 0.5, 0.5),
        'ivory': (0.7, 0.66, 0.52), 'blue': (0.05, 0.1, 0.4), 'dark': (0.03, 0.03, 0.03), 'cover': (0.25, 0.27, 0.2)}
SCREEN = {'ppi': (0.01, 0.02, 0.015), 'range_scope': (0.12, 0.16, 0.1), 'tv': (0.2, 0.26, 0.2), 'elevation_scope': (0.6, 0.5, 0.05),
          'dial': (0.6, 0.55, 0.4), 'meter': (0.75, 0.75, 0.7)}
_mats = {}


def mat(name, rgb, metal=0.0, rough=0.5):
    key = (name, rgb)
    if key not in _mats:
        m = bpy.data.materials.new(name)
        m.use_nodes = True
        bsdf = m.node_tree.nodes['Principled BSDF']
        bsdf.inputs['Base Color'].default_value = (*rgb, 1.0)
        bsdf.inputs['Metallic'].default_value = metal
        bsdf.inputs['Roughness'].default_value = rough
        m.diffuse_color = (*rgb, 1.0)
        _mats[key] = m
    return _mats[key]


def colour(name, default='dark'):
    return LOOK.get(name or default, LOOK[default])


def load_parts():
    with bpy.data.libraries.load(PARTS) as (src, dst):
        dst.objects = [n for n in src.objects]
    parts = {}
    for o in dst.objects:
        parts[o.name] = o.data
    return parts


class Panel:
    def __init__(self, p):
        self.p = p
        c0, n, r, up = B.panel_frame(p)          # body frame, turned into Blender's as the rack's mesh is (B.P)
        self.c0, self.n, self.r, self.up = (B.P(*c0), B.P(*n), B.P(*r), B.P(*up))

    def at(self, u, v, d=0.0):
        return self.c0 + self.r * u + self.up * v + self.n * d

    def basis(self, up=None):
        """Part frame: X right, Y up the face (or `up`), Z out of it."""
        y = (up or self.up).normalized()
        z = self.n
        x = y.cross(z)
        return Matrix((x, y, z)).transposed()


def place(parts, name, panel, u, v, size, rgb, d=0.0, up=None, metal=0.0, coll=None):
    if name not in parts:
        return None
    o = bpy.data.objects.new(name, parts[name])
    (coll or bpy.context.scene.collection).objects.link(o)
    o.matrix_world = Matrix.Translation(panel.at(u, v, d)) @ panel.basis(up).to_4x4() @ Matrix.Diagonal((size, size, size, 1.0))
    o.data = parts[name]
    o.material_slots and None
    o.active_material = mat(name, tuple(rgb), metal)
    if o.material_slots:
        for sl in o.material_slots:
            sl.link = 'OBJECT'
            sl.material = mat(name, tuple(rgb), metal)
    return o


def box(panel, u, v, w, h, d0, d1, rgb, name='box', coll=None, up_rot=0.0):
    import bmesh
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new(name, me)
    (coll or bpy.context.scene.collection).objects.link(o)
    o.matrix_world = (Matrix.Translation(panel.at(u, v, (d0 + d1) / 2)) @ panel.basis().to_4x4() @
                      Matrix.Diagonal((w, h, max(d1 - d0, 1e-4), 1.0)))
    o.active_material = mat(name, tuple(rgb))
    return o


def disc(panel, u, v, w, h, d0, d1, rgb, name='disc', coll=None, seg=40):
    bpy.ops.mesh.primitive_cylinder_add(vertices=seg, radius=0.5, depth=1.0)
    o = bpy.context.active_object
    o.name = name
    for c in o.users_collection:
        c.objects.unlink(o)
    (coll or bpy.context.scene.collection).objects.link(o)
    o.matrix_world = (Matrix.Translation(panel.at(u, v, (d0 + d1) / 2)) @ panel.basis().to_4x4() @
                      Matrix.Diagonal((w, h, max(d1 - d0, 1e-4), 1.0)))
    o.active_material = mat(name, tuple(rgb))
    return o


def face_plane(panel, pid, size):
    me = bpy.data.meshes.new('face_' + pid)
    w, h = size
    pts = [panel.at(-w / 2, -h / 2, 0.0002), panel.at(w / 2, -h / 2, 0.0002), panel.at(w / 2, h / 2, 0.0002),
           panel.at(-w / 2, h / 2, 0.0002)]
    me.from_pydata(pts, [], [(0, 1, 2, 3)])
    uv = me.uv_layers.new()
    for i, (a, b) in enumerate(((0, 0), (1, 0), (1, 1), (0, 1))):
        uv.data[i].uv = (a, b)
    o = bpy.data.objects.new('face_' + pid, me)
    bpy.context.scene.collection.objects.link(o)
    m = bpy.data.materials.new('T_' + pid)
    m.use_nodes = True
    nt = m.node_tree
    tex = nt.nodes.new('ShaderNodeTexImage')
    tex.image = bpy.data.images.load(os.path.join(REPO, 'art', 'cabin_textures', 'T_Panel_%s.png' % pid))
    bsdf = nt.nodes['Principled BSDF']
    nt.links.new(tex.outputs['Color'], bsdf.inputs['Base Color'])
    bsdf.inputs['Roughness'].default_value = 0.6
    o.active_material = m
    return o


def build(layout, console, parts):
    panels = {p['id']: Panel(p) for p in layout['panels'] if p['id'] in PANELS}
    for pid, panel in panels.items():
        face_plane(panel, pid, panel.p['size_m'])
    by_id = {c['id']: c for c in console['controls']}
    styles = {it['id']: it['style'] for it in panel_sheet.build_items(console, layout) if it['what'] == 'control'}
    # ---- controls
    for c in layout['controls']:
        if c['panel'] not in panels:
            continue
        P = panels[c['panel']]
        u, v = c['at_m']
        look = c.get('look', {})
        kind = look.get('kind', '')
        if kind == 'in_mesh':
            continue
        style = styles.get(c['control'], 'button')
        W = look.get('size_m')
        W = W[0] if isinstance(W, list) else W
        cap = kind in ('button', 'lamp_button')
        rgb = colour(look.get('color'), 'dark')
        if style == 'toggle' and not cap:
            Wb = W or 0.016
            place(parts, 'SM_ToggleBase', P, u, v, Wb, LOOK['chrome'], metal=1.0)
            a = math.radians(35)
            lever_up = (P.up * math.cos(a) - P.n * math.sin(a))
            o = place(parts, 'SM_ToggleLever', P, u, v, 0.03, LOOK['chrome'], d=0.42 * Wb, metal=1.0)
            if o:
                y = P.n * math.cos(a) + P.up * math.sin(a)       # the lever's axis (part Z) thrown up the face
                z = y
                x = P.r
                yy = z.cross(x)
                o.matrix_world = (Matrix.Translation(P.at(u, v, 0.42 * Wb)) @ Matrix((x, yy, z)).transposed().to_4x4() @
                                  Matrix.Diagonal((0.03, 0.03, 0.03, 1.0)))
        elif style in ('button', 'toggle') or cap:
            Wb = W or 0.03
            place(parts, 'SM_ButtonRing', P, u, v, Wb, colour(look.get('ring'), 'chrome'), metal=0.8)
            place(parts, 'SM_ButtonCap', P, u, v, Wb, rgb if look.get('color') else LOOK['grey'])
        elif style == 'rotary':
            D = W or 0.026
            name = 'SM_KnobBar' if kind == 'bar_knob' else 'SM_KnobPointer'
            place(parts, name, P, u, v, D, rgb)
        elif style == 'wheel':
            D = W or 0.12
            name = 'SM_WheelDome' if kind == 'dome' else 'SM_WheelDish'
            place(parts, name, P, u, v, D, rgb if look.get('color') else LOOK['dark'], d=0.005)
            hand = max(0.045, 0.4 * D)
            place(parts, 'SM_WheelHandle', P, u, v + 0.4 * D, hand, LOOK['chrome'], d=0.005 + 0.21 * D, metal=1.0)
        elif style == 'key':
            disc(P, u, v, 0.032, 0.032, 0.0, 0.01, LOOK['brass'], 'key')
        elif style == 'cover':
            pass
    # ---- lamps
    for l in layout['lamps']:
        if l['panel'] not in panels:
            continue
        P = panels[l['panel']]
        u, v = l['at_m']
        look = l.get('look', {})
        s = look.get('size_m', 0.022)
        W, H = (s, s) if not isinstance(s, list) else s
        dark = tuple(0.15 * x for x in colour(l.get('color'), 'red'))
        if look.get('kind') == 'legend':
            if abs(W - H) > 1e-6:
                box(P, u, v, W, H, 0.0, 0.012, LOOK['dark'], 'legend')
            else:
                disc(P, u, v, W, W, 0.0, 0.012, LOOK['dark'], 'legend')
        elif look.get('kind') == 'lamp_button':
            place(parts, 'SM_ButtonRing', P, u, v, W, colour(look.get('ring'), 'chrome'), metal=0.8)
            place(parts, 'SM_ButtonCap', P, u, v, W, colour(look.get('color'), 'red'))
        elif abs(W - H) > 1e-6:
            box(P, u, v, W * 1.15, H * 1.4, 0.0, 0.006, LOOK['chrome'], 'lampwin')
            box(P, u, v, W, H, 0.004, 0.01, dark, 'lampglass')
        else:
            place(parts, 'SM_LampBezel', P, u, v, 1.4 * W, LOOK['chrome'], metal=1.0)
            place(parts, 'SM_LampLens', P, u, v, 1.4 * W, colour(look.get('color') or l.get('color'), 'red'))
    # ---- displays: the screen 1.3 cm out, a dark bezel to 1.2 cm
    for d in layout['displays']:
        if d['panel'] not in panels:
            continue
        P = panels[d['panel']]
        u, v = d['at_m']
        w, h = d['size_m']
        rgb = SCREEN.get(d.get('kind', ''), (0.5, 0.5, 0.45))
        if d.get('kind') in ('ppi', 'dial', 'elevation_scope') or abs(w - h) < 1e-6:
            disc(P, u, v, w, h, 0.0, 0.013, rgb, 'screen')
        else:
            box(P, u, v, w, h, 0.0, 0.013, rgb, 'screen')
    # ---- fixtures the game draws
    for fx in layout['fixtures']:
        if fx['panel'] not in panels or fx.get('mesh'):
            continue
        P = panels[fx['panel']]
        u, v = fx['at_m']
        s = fx.get('size_m', 0.02)
        W, H = (s, s) if not isinstance(s, list) else s
        k = fx['kind']
        rgb = colour(fx.get('color'), 'dark')
        parts_for = {'knob': 'SM_Knob', 'pointer_knob': 'SM_KnobPointer', 'bar_knob': 'SM_KnobBar', 'trim': 'SM_Trim',
                     'jack': 'SM_Jack', 'socket': 'SM_Socket'}
        if k in parts_for:
            place(parts, parts_for[k], P, u, v, W, rgb)
        elif k == 'latch':
            place(parts, 'SM_Latch', P, u, v, W, LOOK['chrome'], metal=1.0)
        elif k == 'lamp':
            place(parts, 'SM_LampBezel', P, u, v, 1.4 * W, LOOK['chrome'], metal=1.0)
            place(parts, 'SM_LampLens', P, u, v, 1.4 * W, tuple(0.5 * x for x in rgb))
        elif k in ('button', 'lamp_button'):
            place(parts, 'SM_ButtonRing', P, u, v, W, colour(fx.get('ring'), 'chrome'), metal=0.8)
            place(parts, 'SM_ButtonCap', P, u, v, W, rgb)
        elif k == 'toggle':
            place(parts, 'SM_ToggleBase', P, u, v, W, LOOK['chrome'], metal=1.0)
        elif k == 'nameplate':
            box(P, u, v, W, H, 0.0, 0.0015, (0.45, 0.44, 0.38), 'nameplate')
        elif k == 'bezel':
            box(P, u, v, W, H, 0.0, 0.01, rgb, 'bezel')
        elif k == 'window':
            box(P, u, v, W, H, 0.0, 0.008, rgb, 'window')
        elif k == 'oval':
            disc(P, u, v, W, H, 0.0, 0.003, rgb, 'oval')
        elif k == 'legend':
            disc(P, u, v, W, H, 0.0, 0.012, LOOK['dark'], 'legend')
        elif k == 'gauge':
            disc(P, u, v, W, H, 0.0, 0.01, LOOK['dark'], 'gauge')
    return panels


def render(panels, fits, out_dir):
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_EEVEE' if 'BLENDER_EEVEE' in {e.identifier for e in bpy.types.RenderSettings.bl_rna.properties['engine'].enum_items} else 'BLENDER_EEVEE_NEXT'
    world = bpy.data.worlds.new('w')
    world.use_nodes = True
    world.node_tree.nodes['Background'].inputs['Color'].default_value = (0.55, 0.55, 0.55, 1.0)
    world.node_tree.nodes['Background'].inputs['Strength'].default_value = 0.9
    sc.world = world
    sc.view_settings.view_transform = 'Standard'
    for pid, P in panels.items():
        f = fits[pid]
        a, b, s = f['a'], f['b'], f['s']
        w, h = P.p['size_m']
        cam = bpy.data.cameras.new('cam_' + pid)
        cam.type = 'ORTHO'
        cam.ortho_scale = w
        cam.sensor_fit = 'HORIZONTAL'
        cam.clip_start = 0.01
        cam.clip_end = 2.0
        co = bpy.data.objects.new('cam_' + pid, cam)
        sc.collection.objects.link(co)
        z = P.n
        y = P.up
        x = y.cross(z)
        co.matrix_world = Matrix.Translation(P.at(0, 0, 0.6)) @ Matrix((x, y, z)).transposed().to_4x4()
        sun = bpy.data.lights.new('sun_' + pid, 'SUN')
        sun.energy = 2.5
        sun.angle = math.radians(8)
        so = bpy.data.objects.new('sun_' + pid, sun)
        sc.collection.objects.link(so)
        d = (-P.n * 1.0 + P.up * 0.55 + P.r * 0.25).normalized()      # light from above and a little from the left
        so.matrix_world = Matrix.Translation(P.at(0, 0, 1)) @ d.to_track_quat('-Z', 'Y').to_matrix().to_4x4()
        sc.camera = co
        sc.render.resolution_x = int(round(w / s))
        sc.render.resolution_y = int(round(h / s))
        sc.render.filepath = os.path.join(out_dir, pid + '.png')
        bpy.ops.render.render(write_still=True)
        so.hide_render = True
        win = {'x0': (-w / 2 - a) / s, 'y0': (b - h / 2) / s, 'w': w / s, 'h': h / s}
        with open(os.path.join(out_dir, pid + '.json'), 'w') as fh:
            json.dump(win, fh)


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    fits = json.load(open(argv[argv.index('--fits') + 1]))
    out_dir = argv[argv.index('--out') + 1]
    os.makedirs(out_dir, exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    B.reset()
    layout = B.load_layout()
    with open(os.path.join(REPO, 'data', 'procedures', 'osa_ak.json'), encoding='utf-8') as fh:
        console = json.load(fh)
    B.build_rack('SM_RackSL2M', layout, ['fo_panel', 'so_panel', 'op_panel'], -1, [], 'kick',
                 extra=lambda b, X: (B.sl2m_extra(b, X), B.sl2m_housings(b, layout)), roof_z=B.left_roof_z())
    parts = load_parts()
    panels = build(layout, console, parts)
    if '--blend' in argv:
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out_dir, 'sl2m_check.blend'))
    render(panels, fits, out_dir)


if __name__ == '__main__':
    main()
