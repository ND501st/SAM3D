"""Normalises a third-party model to a map part's frame before Unreal imports it (roadmap T5-T8, docs/scenery/T5_T8_PLAN.md).

    blender --background --python tools/blender/normalize_model.py -- IN.gltf OUT.glb FRAME [--yaw DEG] [--keep-aspect] [--up x|y|z] [--thin TRIS]

FRAME is the part's (scenery/map_meshes.hpp, docs/scenery/ASSETS.md):
  cube  a building or a mass: a unit cube centred on x and y, the cube's base at z = -0.5 (so 1 unit = 1 m: the box's
        scale sets length, width and height); the model is stretched to fill it unless --keep-aspect
  tree  a tree or a plant: one unit tall from its base at z = 0, one across, centred on x and y

All the model's meshes are joined into one object (with their materials), the transforms applied and the model fitted to
the frame exactly (a non-uniform scale: the instance's own scale then sets the simulation's crown width and height), its
pivot baked into the geometry. The result is exported as one mesh in glTF binary (Z up in Blender, converted to Y up by the
exporter, back to Z up by Unreal): the register's own scale stays 1 and its z_offset_m 0. --thin TRIS thins a vegetation model to about that many triangles (a map holds thousands of a tree: roadmap T5, plan
section 13): the twigs and branch cuts are dropped, the trunk and branches decimated to about a third of the budget, and a share
of the leaves kept (each leaf a loose piece of the mesh) and enlarged by the inverse square root of that share, so the crown
keeps its cover. --up names the model's own up axis after the import where it is not Z (a model exported Y-up
without the conversion: its height is its Y); for a tree the default is the longest axis. Prints one line `NORMALIZED
<json>` with the size the model had (metres), for the register's notes.
"""
import json
import math
import random
import sys

import bmesh
import bpy


def args():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    values = {}
    pos = []
    skip = False
    for i, a in enumerate(argv):
        if skip:
            skip = False
        elif a in ('--yaw', '--up', '--thin'):
            values[a] = argv[i + 1]
            skip = True
        elif not a.startswith('--'):
            pos.append(a)
    return pos, float(values.get('--yaw', 0.0)), '--keep-aspect' in argv, values.get('--up', ''), int(values.get('--thin', 0))


def material_name(o):
    return o.data.materials[0].name.lower() if o.data.materials and o.data.materials[0] else ''


def islands(bm):
    """The loose pieces of a bmesh as lists of vertices (union-find over the faces' corners)."""
    parent = {v.index: v.index for v in bm.verts}

    def find(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i

    for f in bm.faces:
        r = find(f.verts[0].index)
        for v in f.verts[1:]:
            parent[find(v.index)] = r
    groups = {}
    for v in bm.verts:
        groups.setdefault(find(v.index), []).append(v)
    return list(groups.values())


def thin_vegetation(meshes, budget):
    """Drops the twigs, decimates the trunk and thins and enlarges the leaves; returns the meshes left."""
    rnd = random.Random(7)
    kept = []
    for o in meshes:
        name = material_name(o)
        if 'twig' in name or 'br_cut' in name:
            bpy.data.objects.remove(o, do_unlink=True)
        else:
            kept.append(o)
    bark = [o for o in kept if 'bark' in material_name(o) or 'trunk' in material_name(o)]
    leaves = [o for o in kept if o not in bark]
    bark_tris = sum(len(o.data.polygons) for o in bark)
    leaf_tris = sum(len(o.data.polygons) for o in leaves)
    trunk_target = min(bark_tris, max(200, budget // 3))
    for o in bark:
        ratio = min(1.0, trunk_target / max(1, bark_tris))
        if ratio < 1.0:
            m = o.modifiers.new('decimate', 'DECIMATE')
            m.ratio = ratio
            bpy.context.view_layer.objects.active = o
            bpy.ops.object.modifier_apply(modifier=m.name)
    share = min(1.0, max(0.005, (budget - sum(len(o.data.polygons) for o in bark)) / max(1, leaf_tris)))
    grow = min(5.0, 1.0 / math.sqrt(share))
    for o in leaves:
        bm = bmesh.new()
        bm.from_mesh(o.data)
        bm.verts.ensure_lookup_table()
        bm.verts.index_update()
        drop = []
        for piece in islands(bm):
            if rnd.random() > share:
                drop.extend(piece)
                continue
            cx = sum(v.co.x for v in piece) / len(piece)
            cy = sum(v.co.y for v in piece) / len(piece)
            cz = sum(v.co.z for v in piece) / len(piece)
            for v in piece:
                v.co.x = cx + (v.co.x - cx) * grow
                v.co.y = cy + (v.co.y - cy) * grow
                v.co.z = cz + (v.co.z - cz) * grow
        bmesh.ops.delete(bm, geom=drop, context='VERTS')
        bm.to_mesh(o.data)
        bm.free()
    left = bark + leaves
    print('THINNED %s' % json.dumps({'budget': budget, 'trunk_tris': sum(len(o.data.polygons) for o in bark), 'leaf_share': round(share, 3),
                                    'leaf_grow': round(grow, 2), 'tris': sum(len(o.data.polygons) for o in left)}))
    return left


def main():
    pos, yaw, keep_aspect, up, thin = args()
    src, dst, frame = pos[0], pos[1], pos[2]
    if frame not in ('cube', 'tree'):
        raise SystemExit('FRAME is cube or tree')
    bpy.ops.wm.read_factory_settings(use_empty=True)
    ext = src.lower().rsplit('.', 1)[-1]
    if ext in ('gltf', 'glb'):
        bpy.ops.import_scene.gltf(filepath=src)
    elif ext == 'fbx':
        bpy.ops.import_scene.fbx(filepath=src)
    elif ext == 'obj':
        bpy.ops.wm.obj_import(filepath=src)
    else:
        raise SystemExit('cannot read ' + ext)
    meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    if not meshes:
        raise SystemExit('no meshes in ' + src)
    bpy.ops.object.select_all(action='DESELECT')
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    # The importer keeps the file's empties as parents (a unit and an axis conversion in them): take the meshes out of
    # them keeping the world transform, so the vertices are the model as it stands.
    bpy.ops.object.parent_clear(type='CLEAR_KEEP_TRANSFORM')
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    if thin:
        meshes = thin_vegetation(meshes, thin)
        bpy.ops.object.select_all(action='DESELECT')
        for o in meshes:
            o.select_set(True)
        bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    if not up and frame == 'tree':
        ext3 = [max(c) - min(c) for c in ([v.co.x for v in obj.data.vertices], [v.co.y for v in obj.data.vertices], [v.co.z for v in obj.data.vertices])]
        up = 'z' if ext3[2] >= 0.95 * max(ext3) else 'xyz'[ext3.index(max(ext3))]  # Z unless another axis is clearly the tall one
    if up == 'y':
        obj.rotation_euler[0] = math.radians(90.0)  # Y to Z
        bpy.ops.object.transform_apply(location=False, rotation=True, scale=False)
    elif up == 'x':
        obj.rotation_euler[1] = math.radians(-90.0)  # X to Z
        bpy.ops.object.transform_apply(location=False, rotation=True, scale=False)
    if yaw:
        obj.rotation_euler[2] = math.radians(yaw)
        bpy.ops.object.transform_apply(location=False, rotation=True, scale=False)
    xs = [v.co.x for v in obj.data.vertices]
    ys = [v.co.y for v in obj.data.vertices]
    zs = [v.co.z for v in obj.data.vertices]
    size = (max(xs) - min(xs), max(ys) - min(ys), max(zs) - min(zs))
    cx, cy, zmin = 0.5 * (max(xs) + min(xs)), 0.5 * (max(ys) + min(ys)), min(zs)
    if min(size) <= 1e-6:
        raise SystemExit('a flat model: %r' % (size,))
    s = (1.0 / size[0], 1.0 / size[1], 1.0 / size[2])
    if keep_aspect:
        u = 1.0 / max(size)
        s = (u, u, u)
    base = -0.5 if frame == 'cube' else 0.0
    for v in obj.data.vertices:
        v.co.x = (v.co.x - cx) * s[0]
        v.co.y = (v.co.y - cy) * s[1]
        v.co.z = (v.co.z - zmin) * s[2] + base
    obj.data.update()
    obj.location = (0.0, 0.0, 0.0)
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.ops.export_scene.gltf(filepath=dst, export_format='GLB', use_selection=True, export_apply=True)
    print('NORMALIZED ' + json.dumps({'in': src, 'out': dst, 'frame': frame, 'up': up or 'z', 'size_m': [round(x, 4) for x in size]}))


main()
