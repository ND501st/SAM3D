"""Render the assembled interior (out/osa_interior/osa_interior.blend, tools/blender/assemble_cabin.py: every mesh, panel,
fixture and control) from a photo's camera, with holistic_scene.py's passes: `model` (Workbench, textures) and `id`
(one flat colour per part, a panel's fixtures and controls counted as the panel). Run by tools/recon/osa/panel_score.py:

    blender --background --factory-startup --python tools/blender/assembled_render.py -- <job.json>

The job: {"blend", "camera" (holistic_view.Camera.blender()), "out" (prefix), "passes", "hide" (part ids), "hinges" ({hinge id:
the share of its folded turn, 1 fully folded}: the vehicle's state in the photo, as the game turns it, e.g. the 0096's cover open
in g097-a), "cabin" (the layout the hinges are read from, data/cabins/osa_ak.json if not given)}.
"""
import json
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import holistic_scene as HS  # noqa: E402


def main():
    job = json.load(open(sys.argv[sys.argv.index('--') + 1], encoding='utf-8'))
    bpy.ops.wm.open_mainfile(filepath=job['blend'])
    sc = bpy.context.scene
    for o in list(sc.objects):
        if o.type == 'CAMERA':
            bpy.data.objects.remove(o, do_unlink=True)
    hide = set(job.get('hide', []))
    objs = []
    for o in list(sc.objects):
        if o.type != 'MESH' or 'part' not in o:
            continue
        part = o['part'][4:] if o['part'].startswith('fix:') else o['part']
        if part in hide:
            bpy.data.objects.remove(o, do_unlink=True)
            continue
        o['part'] = part
        o.hide_render = False
        objs.append(o)
    turn_hinges(job, objs)
    HS.camera(job['camera'])
    HS.render(job['out'], job['passes'], objs, fill=False)


def turn_hinges(job, objs):
    """Turn the parts each named hinge swings (its panels with what is on them, its shell parts) by that share of its folded
    turn about its axis: clockwise seen from the axis's tip (simcore::CabinHinge::Turned), the scene being the body frame."""
    if not job.get('hinges'):
        return
    path = job.get('cabin') or os.path.join(os.path.dirname(os.path.dirname(HERE)), 'data', 'cabins', 'osa_ak.json')
    hinges = {h['id']: h for h in json.load(open(path, encoding='utf-8')).get('hinges', [])}
    for hid, share in job['hinges'].items():
        h = hinges[hid]
        axis = Vector(h.get('axis', [0.0, 0.0, 1.0])).normalized()
        pivot = Vector(h['pivot_m'])
        turn = Matrix.Translation(pivot) @ Matrix.Rotation(-math.radians(h['folded_deg']) * share, 4, axis) @ Matrix.Translation(-pivot)
        parts = set(h.get('panels', [])) | set(h.get('shell', []))
        moved = [o for o in objs if o['part'] in parts]
        for o in moved:
            o.matrix_world = turn @ o.matrix_world
        print('hinge %s: %d objects turned %.1f degrees' % (hid, len(moved), h['folded_deg'] * share))


if __name__ == '__main__':
    main()
