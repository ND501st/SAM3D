"""The whole Osa-AK fighting compartment as one model: every interior mesh where the cabin file places it, every console
panel with its baked face, blocks and fixtures (as holistic_scene.py builds them, which is how USimCabinComponent places
them), and every control, lamp and display standing out of its panel along the panel's normal. For review in Blender or
any FBX viewer; the game itself assembles the same pieces at run time from data/cabins/osa_ak.json, so this file is
never imported into Unreal (it would draw the cabin twice).

    blender --background --factory-startup --python tools/blender/assemble_cabin.py -- [OUT_DIR] [--no-render]

Writes OUT_DIR (default out/osa_interior)/osa_interior.blend, osa_interior.fbx (Blender's frame is the body frame:
x right, y forward, z up, metres; the FBX is exported with -Y forward, Z up), contents.json (every part and how many
objects stand for it) and, unless --no-render, overview renders view_<name>.png. Controls are drawn as simple shapes
(toggle, knob, button, lamp, wheel with its crank, screen) at the cabin file's sizes; Unreal draws them with the parts
in art/cabin_parts/.
"""
import json
import math
import os
import sys

import bmesh
import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(REPO, 'tools'))
import holistic_scene as HS  # noqa: E402
import panel_sheet  # noqa: E402

LOOK = dict(HS.LOOK)
STEEL, DARK, LENS, SCREEN = (0.45, 0.46, 0.48), (0.02, 0.02, 0.02), (0.6, 0.3, 0.03), (0.05, 0.16, 0.08)

VIEWS = {  # name: (eye in the body frame, yaw deg clockwise from forward, pitch deg, lens mm); 'top' is orthographic, roof off
    'aisle_forward': ((0.25, -0.5, 1.70), 2, -10, 14),
    'aisle_aft': ((0.15, 3.3, 1.75), 182, -12, 14),
    'left_consoles': ((0.45, 1.55, 1.62), -90, -6, 14),
    'right_racks': ((-0.30, 1.5, 1.70), 90, -5, 14),
    'cab': ((0.0, 3.0, 1.80), 0, -12, 16),
    'top': ((0.0, 1.75, 6.0), 90, -90, 0),        # forward to the left of the picture, the left side at the bottom
}
ROOF = ('SM_OsaHull', 'SM_OsaGlass', 'SM_DomeLamp')


class Batch:
    """One mesh per panel and colour, so the file stays a few hundred objects."""
    def __init__(self):
        self.bms = {}

    def get(self, pid, key, rgb):
        if (pid, key) not in self.bms:
            self.bms[(pid, key)] = (bmesh.new(), rgb)
        return self.bms[(pid, key)][0]

    def finish(self):
        objs = []
        for (pid, key), (bm, rgb) in self.bms.items():
            o = HS.new_object('item_%s_%s' % (pid, key), bm, HS.material('m_item_%s_%s' % (pid, key), rgb))
            o['part'] = pid
            objs.append(o)
        return objs


def cyl(bm, a, b, radius, seg=16):
    """A closed cylinder from a to b."""
    n = (b - a)
    L = n.length
    n.normalize()
    r = n.orthogonal().normalized()
    u = n.cross(r)
    HS.add_disc(bm, a, n, r, u, 2 * radius, 2 * radius, L, seg)


def lathe(bm, at, n, r, u, prof, seg=32):
    rings = []
    for rr, hh in prof:
        ring = []
        for i in range(seg):
            a = 2 * math.pi * i / seg
            ring.append(bm.verts.new(at + n * hh + (r * math.cos(a) + u * math.sin(a)) * rr))
        rings.append(ring)
    for k in range(len(rings) - 1):
        for i in range(seg):
            j = (i + 1) % seg
            bm.faces.new([rings[k][i], rings[k][j], rings[k + 1][j], rings[k + 1][i]])


def items(cab, console, panels, batch):
    rows = {c['control']: c for c in cab.get('controls', [])}
    lamps = {lp['id']: lp for lp in cab.get('lamps', [])}
    count = 0
    for it in panel_sheet.build_items(console, cab):
        p = panels.get(it['panel'])
        if p is None:
            continue
        n, r, u = HS.panel_axes(p)
        at = Vector(p['centre_m']) + r * it['at'][0] + u * it['at'][1]
        w = it['size'][0]
        h = it['size'][1] if len(it['size']) > 1 else w
        st = it['style']
        pid = it['panel']
        count += 1
        if it['what'] == 'display':
            q = at + n * 0.012
            if st in ('ppi', 'elevation_scope', 'dial'):
                HS.add_disc(batch.get(pid, 'screen', SCREEN), q, n, r, u, w, w, 0.002, 48)
            else:
                HS.add_box(batch.get(pid, 'screen', SCREEN), q, n, r, u, 0.002, w, h)
            continue
        if it['what'] == 'lamp':
            # Dark, as the game shows a lamp that is not lit (USimCabinComponent paints the glass at 4% of its colour):
            # a hint of the colour only, so that a sim lamp and a fixture lamp beside it read alike (PU-303's РАБОТАЕТ).
            colname = (lamps.get(it['id'], {}).get('look') or {}).get('color') or it.get('color') or 'amber'
            lens = tuple(0.1 * c for c in LOOK.get(colname, LENS))
            HS.add_disc(batch.get(pid, 'chrome', STEEL), at, n, r, u, w, w, 0.008)
            HS.add_disc(batch.get(pid, 'lens_' + colname, lens), at + n * 0.008, n, r, u, w / 1.3, w / 1.3, 0.006)
            continue
        look = (rows.get(it['id']) or {}).get('look') or {}
        if look.get('kind') == 'in_mesh':
            continue        # the shell's mesh has the part (the 0096's pairs of buttons); the game keeps only its hit area
        colname = look.get('color')
        col = LOOK.get(colname, DARK)
        key = 'c_' + (colname or 'black')
        if st == 'toggle' and look.get('kind') in ('button', 'lamp_button'):
            st = 'button'   # a switch with a push-button's look (PU-303's ПУСК and ГОМ), as Unreal draws it
        if st == 'toggle':
            HS.add_disc(batch.get(pid, 'chrome', STEEL), at, n, r, u, 0.013, 0.013, 0.006, 6)
            cyl(batch.get(pid, 'chrome', STEEL), at + n * 0.006, at + n * 0.022 + u * 0.007, 0.002, 8)
        elif st in ('rotary', 'key'):
            HS.add_disc(batch.get(pid, 'ring', (0.08, 0.08, 0.08)), at, n, r, u, w + 0.008, w + 0.008, 0.004, 24)
            HS.add_disc(batch.get(pid, key, col), at + n * 0.004, n, r, u, w, w, 0.016, 24)
            HS.add_box(batch.get(pid, 'mark', LOOK['white']), at + n * 0.021 + u * w * 0.25, n, r, u, 0.002, 0.003, w * 0.5)
        elif st == 'button':
            HS.add_disc(batch.get(pid, 'chrome', STEEL), at, n, r, u, w, w, 0.008, 24)
            HS.add_disc(batch.get(pid, key, col), at + n * 0.008, n, r, u, w / 1.4, w / 1.4, 0.006, 24)
        elif st == 'wheel':
            D = w
            if look.get('kind') == 'dome':
                prof = [(0.5 * D, 0.0), (0.5 * D, 0.05 * D), (0.49 * D, 0.09 * D), (0.46 * D, 0.14 * D), (0.4 * D, 0.21 * D),
                        (0.32 * D, 0.28 * D), (0.22 * D, 0.35 * D), (0.12 * D, 0.40 * D), (0.001, 0.42 * D)]
            else:
                prof = [(0.12 * D, 0.0), (0.12 * D, 0.06 * D), (0.4 * D, 0.1 * D), (0.47 * D, 0.16 * D), (0.5 * D, 0.22 * D),
                        (0.48 * D, 0.26 * D), (0.44 * D, 0.24 * D), (0.38 * D, 0.17 * D), (0.12 * D, 0.13 * D), (0.001, 0.13 * D)]
            bm = batch.get(pid, key, col)
            lathe(bm, at + n * 0.005, n, r, u, prof, 48)
            L = max(0.045, 0.4 * D)
            hb = at + n * (0.005 + 0.21 * D) - u * (0.4 * D)        # the crank at the bottom of its turn
            cyl(bm, hb, hb + n * 0.3 * L, 0.12 * L)
            cyl(bm, hb + n * 0.3 * L, hb + n * L, 0.21 * L)
        elif st == 'cover':
            HS.add_box(batch.get(pid, 'cover', LOOK['red']), at + n * 0.02, n, r, u, 0.004, w, h)
        else:
            HS.add_disc(batch.get(pid, key, col), at, n, r, u, w, w, 0.015, 20)
    return count


def camera(eye, yaw, pitch, lens):
    sc = bpy.context.scene
    cam = sc.camera
    if cam is None:
        cam = bpy.data.objects.new('view_cam', bpy.data.cameras.new('view_cam'))
        sc.collection.objects.link(cam)
        sc.camera = cam
    cam.location = Vector(eye)
    cam.rotation_euler = (math.radians(90 + pitch), 0.0, math.radians(-yaw))   # a Blender camera looks down -Z; yaw 0 looks along +y (forward)
    cam.data.clip_start = 0.01
    if lens:
        cam.data.type, cam.data.lens = 'PERSP', lens
    else:
        cam.data.type, cam.data.ortho_scale = 'ORTHO', 5.6


def render_views(out, roof):
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_WORKBENCH'
    sc.render.resolution_x, sc.render.resolution_y = 1600, 900
    sc.view_settings.view_transform = 'Standard'
    sh = sc.display.shading
    sh.light, sh.color_type, sh.show_cavity = 'STUDIO', 'TEXTURE', False     # world cavity draws long thin triangles as streaks
    sh.show_shadows = False      # Workbench's shadows draw thin parts' edges as white streaks across the cabin
    sc.display.render_aa = '8'
    paths = []
    for name, (eye, yaw, pitch, lens) in VIEWS.items():
        for o in roof:      # the windscreens' and periscopes' glass is left out of every view: Workbench draws its scratches as streaks
            o.hide_render = name == 'top' or o.get('glass', False)
        camera(eye, yaw, pitch, lens)
        sc.render.filepath = os.path.join(out, 'view_%s.png' % name)
        bpy.ops.render.render(write_still=True)
        paths.append(sc.render.filepath)
    for o in roof:
        o.hide_render = False
    return paths


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    flags = [a for a in argv if a.startswith('--')]
    out = os.path.abspath(([a for a in argv if not a.startswith('--')] or [os.path.join(REPO, 'out', 'osa_interior')])[0])
    os.makedirs(out, exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    cab = json.load(open(os.path.join(REPO, 'data', 'cabins', 'osa_ak.json'), encoding='utf-8'))
    console = json.load(open(os.path.join(REPO, 'data', 'procedures', 'osa_ak.json'), encoding='utf-8'))
    objs = HS.build(cab, REPO)
    panels = {p['id']: p for p in cab['panels']}
    batch = Batch()
    n_items = items(cab, console, panels, batch)
    objs += batch.finish()
    mesh_of = {s['id']: s['mesh'] for s in cab['shell'] if s.get('kind') == 'mesh'}
    missing = sorted({m for m in mesh_of.values() if not os.path.exists(os.path.join(REPO, 'art', 'cabin_interior', m + '.fbx'))})
    parts = {}
    for o in objs:
        parts[o['part']] = parts.get(o['part'], 0) + 1
    contents = {'objects': len(objs), 'items_on_panels': n_items, 'meshes_placed': sorted(set(mesh_of.values())),
                'meshes_missing': missing, 'panels': sorted(panels), 'parts': dict(sorted(parts.items()))}
    with open(os.path.join(out, 'contents.json'), 'w', encoding='utf-8', newline='\n') as f:
        json.dump(contents, f, indent=1, ensure_ascii=False)
    print('assemble_cabin: %d objects, %d meshes placed, %d panels, %d items; missing: %s'
          % (len(objs), len(contents['meshes_placed']), len(panels), n_items, missing or 'none'))
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs:
        o.select_set(True)
    bpy.ops.export_scene.fbx(filepath=os.path.join(out, 'osa_interior.fbx'), use_selection=True, axis_forward='-Y', axis_up='Z',
                             apply_scale_options='FBX_SCALE_UNITS', path_mode='COPY', embed_textures=True)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, 'osa_interior.blend'))
    if '--no-render' not in flags:
        roof = [o for o in objs if mesh_of.get(o['part']) in ROOF]
        for o in roof:
            o['glass'] = mesh_of.get(o['part']) == 'SM_OsaGlass'
        for p in render_views(out, roof):
            print('assemble_cabin: wrote', p)
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, 'osa_interior.blend'))
    return 1 if missing else 0


if __name__ == '__main__':
    sys.exit(main())
