"""Render the assembled interior (out/osa_interior/osa_interior.blend, tools/blender/assemble_cabin.py: every mesh, panel,
fixture and control) from a photo's camera, with holistic_scene.py's passes: `model` (Workbench, textures) and `id`
(one flat colour per part, a panel's fixtures and controls counted as the panel). Run by tools/recon/osa/panel_score.py:

    blender --background --factory-startup --python tools/blender/assembled_render.py -- <job.json>

The job: {"blend", "camera" (holistic_view.Camera.blender()), "out" (prefix), "passes", "hide" (part ids)}.
"""
import json
import os
import sys

import bpy

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
    HS.camera(job['camera'])
    HS.render(job['out'], job['passes'], objs, fill=False)


if __name__ == '__main__':
    main()
