"""Stage S10: the game model, built headless in Blender from the fitted trace (docs/OSA_EXTERIOR_RECONSTRUCTION.md).

    blender --background --factory-startup --python tools/recon/blender_build.py -- \
        --config tools/recon/osa_9a33bm2.json --run out/recon/osa_9a33bm2 --out out/recon/osa_9a33bm2/build

What it builds, in the vehicle frame (x right, y forward, z up, metres; Unreal's import turns it):
* the hull from the plate trace (`sources.hull_trace`: planar plates as polygons, each from its photos): one mesh, the
  plates welded at their shared edges, so its edges are the plates' real intersections, not fitted curves. Until the
  trace exists, the reference mesh's hull part stands in, scaled by the fit (and the report says so);
* the wheels from the tyre's sourced size and the axle stations (`dimensions`): a parametric tyre (tread blocks as an
  array) and rim, the same for all six, instead of the reference mesh's noisy or wrong ones;
* the reference mesh's other parts (turret, antennas, containers) as stand-ins, each scaled by the fit, in a
  collection named for replacement part by part;
then UVs (cube projection per plate at one texel density), LOD1-2 by decimation, FBX export, and `report.json`
(counts, sizes, what stood in) and `hull_faces.json` (for the CAD stage).
"""
import json
import math
import os
import sys

import bpy
import bmesh


def args():
    a = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    out = {}
    for i in range(0, len(a) - 1, 2):
        out[a[i].lstrip('-')] = a[i + 1]
    return out


def clear():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def collection(name):
    c = bpy.data.collections.get(name) or bpy.data.collections.new(name)
    if c.name not in bpy.context.scene.collection.children:
        bpy.context.scene.collection.children.link(c)
    return c


def mesh_object(name, verts, faces, coll):
    me = bpy.data.meshes.new(name)
    me.from_pydata(verts, [], faces)
    me.validate()
    me.update()
    ob = bpy.data.objects.new(name, me)
    coll.objects.link(ob)
    return ob


def hull_from_trace(trace, coll):
    """Plates (planar polygons) welded into one mesh; returns the object and the faces for the CAD stage."""
    verts, faces, out_faces = [], [], []
    for plate in trace['plates']:
        idx = []
        for p in plate['points_m']:
            verts.append(tuple(p))
            idx.append(len(verts) - 1)
        faces.append(idx)
        out_faces.append({'name': plate['name'], 'points_m': plate['points_m']})
    ob = mesh_object('SM_Osa_Hull', verts, faces, coll)
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=trace.get('weld_m', 0.004))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(ob.data)
    bm.free()
    return ob, out_faces


def tyre(name, radius, width, rim_r, coll, segments=48, blocks=24):
    """A parametric tyre and rim on the x axis (wheels turn about x): torus-like section, tread blocks, rim dish."""
    bm = bmesh.new()
    rings = []
    section = [(-width / 2, rim_r), (-width / 2, radius - 0.06), (-width / 2 + 0.04, radius),
               (width / 2 - 0.04, radius), (width / 2, radius - 0.06), (width / 2, rim_r)]
    for i in range(segments):
        a = 2 * math.pi * i / segments
        ring = [bm.verts.new((x, r * math.cos(a), r * math.sin(a))) for x, r in section]
        rings.append(ring)
    for i in range(segments):
        r0, r1 = rings[i], rings[(i + 1) % segments]
        for k in range(len(section) - 1):
            bm.faces.new((r0[k], r1[k], r1[k + 1], r0[k + 1]))
    # tread blocks: raised quads on the crown, alternating sides like a cross-country tyre
    for j in range(blocks):
        a0 = 2 * math.pi * (j + 0.1) / blocks
        a1 = 2 * math.pi * (j + 0.55) / blocks
        side = -1 if j % 2 else 1
        x0, x1 = (0.0, side * (width / 2 - 0.02))
        h = 0.025
        pts = []
        for a in (a0, a1):
            for x in (x0, x1):
                pts.append((x, (radius + h) * math.cos(a), (radius + h) * math.sin(a)))
        v = [bm.verts.new(p) for p in pts]
        bm.faces.new((v[0], v[1], v[3], v[2]))
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    coll.objects.link(ob)
    # rim dish
    bpy.ops.mesh.primitive_cylinder_add(vertices=32, radius=rim_r, depth=width * 0.6, rotation=(0, math.pi / 2, 0))
    rim = bpy.context.active_object
    rim.name = name + '_Rim'
    for c in rim.users_collection:
        c.objects.unlink(rim)
    coll.objects.link(rim)
    rim.parent = ob
    return ob


def import_reference(obj_path, axes, front_sign, scale, coll):
    """The reference mesh as stand-in parts, in the vehicle frame and scaled by the fit."""
    before = set(bpy.data.objects)
    # the config's axes say which signed file axis is the vehicle's forward and up; Blender's importer takes the
    # same (probed on the reference: forward Z, up Y puts its bow at +Y and its left lamps at -X)
    name = lambda a: ('NEGATIVE_' if a.startswith('-') else '') + a.lstrip('-').upper()
    fwd, up = name((axes or ['-x', 'z', 'y'])[1]), name((axes or ['-x', 'z', 'y'])[2])
    if hasattr(bpy.ops.wm, 'obj_import'):
        bpy.ops.wm.obj_import(filepath=obj_path, forward_axis=fwd, up_axis=up)
    else:
        bpy.ops.import_scene.obj(filepath=obj_path, axis_forward=fwd.replace('NEGATIVE_', '-'), axis_up=up.replace('NEGATIVE_', '-'))
    new = [o for o in bpy.data.objects if o not in before]
    for o in new:
        for c in o.users_collection:
            c.objects.unlink(o)
        coll.objects.link(o)
    # the remaining half turn (a bow at the file's negative end) and the fit's scale go on a parent empty
    root = bpy.data.objects.new('Reference_Root', None)
    coll.objects.link(root)
    root.rotation_euler = (0, 0, math.pi if front_sign < 0 else 0)
    root.scale = scale
    for o in new:
        o.parent = root
    return root, new


def lods(ob, coll, ratios=(0.5, 0.25)):
    out = []
    for i, r in enumerate(ratios, start=1):
        c = ob.copy()
        c.data = ob.data.copy()
        c.name = '%s_LOD%d' % (ob.name, i)
        coll.objects.link(c)
        m = c.modifiers.new('decimate', 'DECIMATE')
        m.ratio = r
        out.append(c)
    return out


def uv_cube(ob, texel_m=0.5):
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.cube_project(cube_size=texel_m)
    bpy.ops.object.mode_set(mode='OBJECT')
    ob.select_set(False)


def main():
    a = args()
    cfg = json.load(open(a['config'], encoding='utf-8'))
    run = a['run']
    out = a['out']
    os.makedirs(out, exist_ok=True)
    repo = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    rp = lambda p: p if os.path.isabs(p) else os.path.join(repo, p)
    clear()
    report = {'stand_ins': [], 'objects': {}}
    fit = json.load(open(os.path.join(run, 'fit.json'))) if os.path.exists(os.path.join(run, 'fit.json')) else {'scale': [1, 1, 1]}
    game = collection('Osa_Game')
    ref = collection('Osa_Reference')
    dims = cfg.get('dimensions', {})

    trace_path = cfg['sources'].get('hull_trace')
    if trace_path and os.path.exists(rp(trace_path)):
        trace = json.load(open(rp(trace_path), encoding='utf-8'))
        hull, faces = hull_from_trace(trace, game)
        json.dump({'faces': faces}, open(os.path.join(out, 'hull_faces.json'), 'w'), indent=1)
        uv_cube(hull)
        lods(hull, game)
    else:
        report['stand_ins'].append('hull: the reference mesh (no plate trace yet)')

    # wheels: the sourced tyre on the sourced axle stations
    tyre_d = dims.get('tyre_outer_diameter_m', {}).get('value')
    tyre_w = dims.get('tyre_width_m', {}).get('value')
    axles = dims.get('axle_stations_m', {}).get('value')
    track = dims.get('track_m', {}).get('value')
    if tyre_d and tyre_w and axles and track:
        for i, y in enumerate(axles):
            for side, sx in (('L', -1), ('R', 1)):
                t = tyre('SM_Osa_Wheel_%d%s' % (i + 1, side), tyre_d / 2, tyre_w, dims.get('rim_radius_m', {}).get('value', 0.27), game)
                t.location = (sx * track / 2, y, tyre_d / 2 - dims.get('tyre_static_deflection_m', {}).get('value', 0.0))
    else:
        report['stand_ins'].append('wheels: the reference mesh (tyre size, axle stations or track not sourced yet)')

    src = cfg['sources']['reference_mesh']
    if os.path.exists(rp(src['obj'])):
        root, parts = import_reference(rp(src['obj']), src.get('axes'), src.get('front_sign', 1.0), fit['scale'], ref)
        report['stand_ins'].append('turret, antennas, containers, fittings: the reference mesh (%d objects), scaled %s' % (len(parts), fit['scale']))
        for o in parts:  # never export the reference: it is a stand-in to be replaced and its licence is unknown
            o.hide_render = True
    for o in game.objects:
        if o.type == 'MESH':
            report['objects'][o.name] = {'verts': len(o.data.vertices), 'faces': len(o.data.polygons),
                                         'size_m': [round(v, 3) for v in o.dimensions]}
    bpy.ops.object.select_all(action='DESELECT')
    for o in game.objects:
        o.select_set(True)
    if game.objects:
        bpy.ops.export_scene.fbx(filepath=os.path.join(out, 'SM_Osa_9A33BM2.fbx'), use_selection=True,
                                 apply_unit_scale=True, axis_forward='Y', axis_up='Z', mesh_smooth_type='FACE')
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, 'osa_9a33bm2.blend'))
    json.dump(report, open(os.path.join(out, 'report.json'), 'w'), indent=1)
    print('BUILD OK', json.dumps(report)[:400])


main()
