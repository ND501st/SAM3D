"""Prepare a model downloaded from Sketchfab for the game (docs/scenery/ASSETS.md, tools/sketchfab/models.json).

    blender --background --python tools/blender/sketchfab_prepare.py -- <asset id> [<asset id> ...]
    blender --background --python tools/blender/sketchfab_prepare.py -- --all
    blender --background --python tools/blender/sketchfab_prepare.py -- <asset id> --probe

For each asset in tools/sketchfab/models.json whose source file (art/thirdparty/sketchfab/<id>/source.glb, fetched by
tools/sketchfab_fetch.py) is on this machine (SAM3D_SKETCHFAB_ART names another folder of them):

1. import the glTF, bake each mesh as shown (its modifiers, an armature's pose), drop what the game must not draw (the
   spec's "drop": name substrings of objects, parents, meshes or materials, case-insensitive) and everything that is
   not a mesh, and join the rest into one mesh, every transform applied; "keep" ({"axis", "min", "max"}, the file's
   frame) keeps only the loose parts whose centre lies in a range (one copy of a file that holds two, a round without
   its launcher);
2. turn it into the model's frame (x forward, y left in Blender = right in Unreal, z up): "auto_yaw" first turns a model
   posed at an angle in its file by its plane of symmetry (and "yaw_deg" more), then "forward" and "up" name the axes
   ("+x", "-y", ...) the nose and the top point along;
3. scale it so its length along x is the game's (the basic model's or the munition's length_m, data/scenery/; a
   helicopter's, "main_rotor", without its main rotor's blades, which may overhang: long thin high loose parts, and with
   "blade_band" everything in that share of the height below the top), and put the origin at the middle of the fuselage
   or the body: x at the middle of its length, y on its plane of symmetry, z on the fuselage's axis from the vertices
   in a slab across the middle (not the bounding box, which the fin lifts), a munition's on the line of its nose's tip;
4. for an aircraft that flies with its gear up, "gear_clear_m": delete the loose parts whose bottom lies within that
   height of the tyres' bottoms and no longer than "gear_part_m" (a quarter of the length if not given), then place it
   again (a nose probe may go too);
5. merge the materials that look the same (the same images and colour), bring it under its triangle budget ("max_tris",
   a collapse decimate) and its textures under "texture_px";
6. write art/thirdparty/sketchfab/<id>/SM_<Name>.glb (what tools/ue_import_sketchfab.py imports) and a check render,
   art/thirdparty/sketchfab/<id>/check.png: side (nose to the right), top and front, orthographic, with the origin's
   axes (red x forward, green y left, blue z up). Look at it: the nose to the right, the origin on the axis.

A spec without "forward", or --probe, renders the file as imported (probe.png, its own axes, after auto_yaw) and lists
its objects, to read "forward", "up", "drop" and "keep" off.

Everything it writes is under art/thirdparty/ (ignored by git); the register, data/scenery/assets.json, says where each
model came from and under which licence.
"""
import json
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
MANIFEST = os.path.join(REPO, 'tools', 'sketchfab', 'models.json')
# The fetched files: this checkout's art/thirdparty/sketchfab, or SAM3D_SKETCHFAB_ART (one copy shared by worktrees).
ART = os.environ.get('SAM3D_SKETCHFAB_ART') or os.path.join(REPO, 'art', 'thirdparty', 'sketchfab')

AXES = {'+x': Vector((1, 0, 0)), '-x': Vector((-1, 0, 0)), '+y': Vector((0, 1, 0)), '-y': Vector((0, -1, 0)),
        '+z': Vector((0, 0, 1)), '-z': Vector((0, 0, -1))}


def game_lengths():
    """Every basic model's and munition's length in metres, by id."""
    out = {}
    with open(os.path.join(REPO, 'data', 'scenery', 'basic_models.json'), encoding='utf-8') as f:
        for m in json.load(f)['models']:
            if 'length_m' in m:
                out['model:' + m['id']] = m['length_m']
    with open(os.path.join(REPO, 'data', 'scenery', 'munitions.json'), encoding='utf-8') as f:
        for m in json.load(f)['munitions']:
            if m.get('length_m'):
                out['munition:' + m['id']] = m['length_m']
    return out


def clear_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    for block in (bpy.data.meshes, bpy.data.materials, bpy.data.images, bpy.data.objects):
        for item in list(block):
            block.remove(item)


def is_dropped(obj, drop):
    names = [obj.name.lower()]
    p = obj.parent
    while p is not None:
        names.append(p.name.lower())
        p = p.parent
    if obj.type == 'MESH' and obj.data is not None:
        names.append(obj.data.name.lower())
        names += [s.material.name.lower() for s in obj.material_slots if s.material is not None]
    return any(d.lower() in n for d in drop for n in names)


def join_meshes(drop):
    meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH' and not is_dropped(o, drop)]
    dropped = sorted({o.name for o in bpy.context.scene.objects if o.type == 'MESH' and o not in meshes})
    if not meshes:
        raise RuntimeError('nothing left to draw after the drops')
    # Make every mesh its own data as shown (its modifiers baked in, an armature's pose among them, before the armature
    # goes) with its world transform applied, then join.
    dg = bpy.context.evaluated_depsgraph_get()
    for o in meshes:
        o.data = bpy.data.meshes.new_from_object(o.evaluated_get(dg), preserve_all_data_layers=True, depsgraph=dg)
        o.modifiers.clear()
    for o in meshes:
        o.data.transform(o.matrix_world)
        o.parent = None
        o.matrix_world = Matrix.Identity(4)
    for o in list(bpy.context.scene.objects):
        if o not in meshes:
            bpy.data.objects.remove(o, do_unlink=True)
    bpy.ops.object.select_all(action='DESELECT')
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    return bpy.context.view_layer.objects.active, dropped


def symmetry_yaw(obj, samples=2500):
    """The yaw (degrees) that turns the model's vertical plane of symmetry onto x-z: the angle of the plane's direction
    from +x, in [0, 180), found by mirroring a sample of the vertices across each candidate plane (through their mean)
    and measuring how far each mirrored vertex lies from the nearest real one. For a model posed turned in its file."""
    from mathutils import kdtree
    vs = [v.co.copy() for v in obj.data.vertices]
    tree = kdtree.KDTree(len(vs))
    for i, v in enumerate(vs):
        tree.insert(v, i)
    tree.balance()
    step = max(1, len(vs) // samples)
    sample = vs[::step]
    mean = sum(sample, Vector()) / len(sample)

    def cost(deg):
        t = math.radians(deg)
        n = Vector((-math.sin(t), math.cos(t), 0.0))  # the plane's normal
        total = 0.0
        for v in sample:
            d = (v - mean).dot(n)
            total += tree.find(v - 2.0 * d * n)[2]
        return total

    best = min((cost(a), a) for a in range(0, 180, 2))[1]
    for span, stepd in ((2.0, 0.25), (0.25, 0.02)):
        cands = [best + k * stepd for k in range(int(-span / stepd), int(span / stepd) + 1)]
        best = min((cost(a), a) for a in cands)[1]
    return best % 180.0


def loose_parts(bm):
    """The mesh's loose parts, each a list of its vertices."""
    bm.verts.ensure_lookup_table()
    seen, parts = set(), []
    for v in bm.verts:
        if v.index in seen:
            continue
        part, stack = [], [v]
        seen.add(v.index)
        while stack:
            u = stack.pop()
            part.append(u)
            for e in u.link_edges:
                w = e.other_vert(u)
                if w.index not in seen:
                    seen.add(w.index)
                    stack.append(w)
        parts.append(part)
    return parts


def keep_parts(obj, keep):
    """Keep only the loose parts whose centre lies within the spec's "keep" ({"axis": "x", "min": a, "max": b}, the
    source's frame as imported, metres): one copy of a file that holds two, or the round without its launcher."""
    i = 'xyz'.index(keep['axis'])
    lo, hi = keep.get('min', -math.inf), keep.get('max', math.inf)
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    doomed = []
    for part in loose_parts(bm):
        c = sum(p.co[i] for p in part) / len(part)
        if not lo <= c <= hi:
            doomed += part
    bmesh.ops.delete(bm, geom=doomed, context='VERTS')
    bm.to_mesh(obj.data)
    bm.free()
    return len(doomed)


def drop_gear(obj, clear_m, max_part_m):
    """Delete the loose parts (tyres, struts, doors) whose lowest point lies within `clear_m` of the aircraft's lowest
    point (the tyres' bottoms, metres) and that are no longer or wider than `max_part_m`: the gear, down in the file, in
    an aircraft that flies with it up. A strut that reaches up into the wing goes whole, by its bottom."""
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    floor = min(v.co.z for v in bm.verts) + clear_m
    doomed = []
    for part in loose_parts(bm):
        xs = [p.co.x for p in part]
        ys = [p.co.y for p in part]
        if min(p.co.z for p in part) < floor and max(xs) - min(xs) <= max_part_m and max(ys) - min(ys) <= max_part_m:
            doomed += part
    n = len(doomed)
    bmesh.ops.delete(bm, geom=doomed, context='VERTS')
    bm.to_mesh(obj.data)
    bm.free()
    return n


def straighten(obj, spec):
    """Turn a model posed turned in its file: by its plane of symmetry ("auto_yaw": true) and/or a given "yaw_deg" more,
    about the vertical. Returns the total turn in degrees."""
    yaw = symmetry_yaw(obj) if spec.get('auto_yaw') else 0.0
    yaw += spec.get('yaw_deg', 0.0)
    if yaw:
        obj.data.transform(Matrix.Rotation(math.radians(-yaw), 4, 'Z'))
    return round(yaw, 2)


def orient(obj, forward, up):
    f, u = AXES[forward], AXES[up]
    if abs(f.dot(u)) > 1e-6:
        raise RuntimeError('forward and up must be different axes')
    left = u.cross(f)  # Blender: x forward, z up, so y is left
    # Rows: the source's forward, left and up become x, y, z.
    rot = Matrix((f, left, u)).to_4x4()
    obj.data.transform(rot)


def blade_free_x(obj, band=0.0):
    """The x extent of a helicopter without its main rotor's blades (the game's length is the fuselage's; the blades,
    long, thin and high, reach past the nose and the tail): loose parts longer than a quarter of the whole, thinner than
    a twentieth of it and wholly above the middle height are left out; with a `band` (a share of the height), so is
    everything in that band below the top (blades joined to the hub, or drooping)."""
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    xs_all = [v.co.x for v in bm.verts]
    zs_all = sorted(v.co.z for v in bm.verts)
    whole = max(xs_all) - min(xs_all)
    z_mid = 0.5 * (zs_all[0] + zs_all[-1])
    z_cut = zs_all[-1] - band * (zs_all[-1] - zs_all[0])
    xs = []
    blades = 0
    for part in loose_parts(bm):
        px = [p.co.x for p in part]
        py = [p.co.y for p in part]
        pz = [p.co.z for p in part]
        long_ = max(max(px) - min(px), max(py) - min(py)) > 0.25 * whole
        if long_ and max(pz) - min(pz) < 0.05 * whole and min(pz) > z_mid:
            blades += 1
            continue
        xs += [p.co.x for p in part if band <= 0.0 or p.co.z < z_cut]
    bm.free()
    return min(xs), max(xs), blades


def place(obj, length_m, slab=0.06, axis_from_nose=False, without_blades=False, blade_band=0.0):
    vs = [v.co.copy() for v in obj.data.vertices]
    xs = [v.x for v in vs]
    x0, x1 = min(xs), max(xs)
    if without_blades:
        x0, x1, _ = blade_free_x(obj, blade_band)
    src_len = x1 - x0
    s = length_m / src_len
    obj.data.transform(Matrix.Scale(s, 4))
    vs = [v * s for v in vs]
    xm = 0.5 * (x0 + x1) * s
    ys = [v.y for v in vs]
    ym = 0.5 * (min(ys) + max(ys))
    if axis_from_nose:
        # A munition: its nose's tip lies on the body's axis (a slab across the middle catches the lugs and the wings).
        tip = [v for v in vs if v.x > x1 * s - 0.01 * length_m]
        zm = sum(v.z for v in tip) / len(tip)
        ym = sum(v.y for v in tip) / len(tip)
        obj.data.transform(Matrix.Translation((-xm, -ym, -zm)))
        return s
    # The fuselage's (body's) axis: the vertices in a slab across the middle, near the plane of symmetry.
    half = slab * length_m
    near = [v for v in vs if abs(v.x - xm) < half and abs(v.y - ym) < 0.12 * length_m]
    if len(near) < 8:
        near = [v for v in vs if abs(v.x - xm) < 3 * half]
    zs = [v.z for v in near] or [v.z for v in vs]
    zm = 0.5 * (min(zs) + max(zs))
    obj.data.transform(Matrix.Translation((-xm, -ym, -zm)))
    return s


def material_key(mat):
    """What a material looks like: the images its nodes read and its base colour, alpha and blend. Two materials with
    the same key draw the same (the files carry one material per part, mostly all reading one texture)."""
    if mat is None:
        return ('none',)
    images, colour = [], tuple(round(c, 3) for c in mat.diffuse_color)
    if mat.use_nodes and mat.node_tree is not None:
        for node in mat.node_tree.nodes:
            if node.type == 'TEX_IMAGE' and node.image is not None:
                images.append(node.image.name)
            if node.type == 'BSDF_PRINCIPLED':
                base = node.inputs['Base Color']
                if not base.is_linked:
                    colour = tuple(round(c, 3) for c in base.default_value)
                alpha = node.inputs['Alpha']
                colour += (round(alpha.default_value, 3), alpha.is_linked)
    return (tuple(sorted(images)), colour, getattr(mat, 'surface_render_method', ''))


def merge_materials(obj):
    """Point every face at the first material that looks the same, then drop the slots no face uses."""
    slots = [s.material for s in obj.material_slots]
    first = {}
    remap = []
    for i, mat in enumerate(slots):
        remap.append(first.setdefault(material_key(mat), i))
    for poly in obj.data.polygons:
        poly.material_index = remap[poly.material_index]
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.material_slot_remove_unused()
    return len(slots), len(obj.material_slots)


def triangles(obj):
    return sum(len(p.vertices) - 2 for p in obj.data.polygons)


def decimate(obj, max_tris):
    tris = triangles(obj)
    if tris <= max_tris:
        return tris
    mod = obj.modifiers.new('budget', 'DECIMATE')
    mod.decimate_type = 'COLLAPSE'
    mod.ratio = max_tris / tris
    mod.use_collapse_triangulate = True
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=mod.name)
    return triangles(obj)


def shrink_textures(max_px):
    for img in bpy.data.images:
        if img.size[0] > max_px or img.size[1] > max_px:
            k = max_px / max(img.size[0], img.size[1])
            img.scale(max(1, int(img.size[0] * k)), max(1, int(img.size[1] * k)))
        if img.packed_file is None and img.has_data:
            img.pack()


def check_render(obj, path, length_m):
    """Side (from the right, nose to the right of the picture... from Unreal's +y), top and front views in one image."""
    scene = bpy.context.scene
    scene.render.engine = 'BLENDER_WORKBENCH'
    scene.display.shading.light = 'STUDIO'
    scene.display.shading.color_type = 'TEXTURE'
    scene.render.resolution_x, scene.render.resolution_y = 1200, 400
    scene.render.film_transparent = False
    world = bpy.data.worlds.new('w')
    scene.world = world
    # Axes at the origin: a red rod along +x (the nose), green along +y (left), blue along +z (up).
    for axis, colour in (('x', (1, 0, 0, 1)), ('y', (0, 1, 0, 1)), ('z', (0, 0, 1, 1))):
        bpy.ops.mesh.primitive_cylinder_add(radius=0.01 * length_m, depth=0.25 * length_m)
        rod = bpy.context.active_object
        if axis == 'x':
            rod.rotation_euler = (0, math.pi / 2, 0)
            rod.location = (0.125 * length_m, 0, 0)
        elif axis == 'y':
            rod.rotation_euler = (math.pi / 2, 0, 0)
            rod.location = (0, 0.125 * length_m, 0)
        else:
            rod.location = (0, 0, 0.125 * length_m)
        mat = bpy.data.materials.new('axis_' + axis)
        mat.diffuse_color = colour
        rod.data.materials.append(mat)
    cam_data = bpy.data.cameras.new('cam')
    cam_data.type = 'ORTHO'
    cam_data.ortho_scale = 1.15 * length_m
    cam_data.clip_end = 10.0 * length_m  # a file in millimetres is thousands of units long
    cam = bpy.data.objects.new('cam', cam_data)
    scene.collection.objects.link(cam)
    scene.camera = cam
    views = {
        'side': ((0, -3 * length_m, 0), (math.pi / 2, 0, 0)),     # from the right: nose to the right
        'top': ((0, 0, 3 * length_m), (0, 0, 0)),                # from above: nose to the right
        'front': ((3 * length_m, 0, 0), (math.pi / 2, 0, math.pi / 2)),
    }
    shots = []
    base = os.path.splitext(path)[0]
    scene.render.resolution_x, scene.render.resolution_y = 600, 400
    for name, (loc, rot) in views.items():
        cam.location, cam.rotation_euler = loc, rot
        scene.render.filepath = '%s_%s.png' % (base, name)
        bpy.ops.render.render(write_still=True)
        shots.append(scene.render.filepath)
    # Paste the three side by side.
    imgs = [bpy.data.images.load(p) for p in shots]
    w, h = imgs[0].size
    out = bpy.data.images.new('check', 3 * w, h)
    px = [0.0] * (3 * w * h * 4)
    for k, im in enumerate(imgs):
        src = list(im.pixels)
        for row in range(h):
            a = row * w * 4
            b = (row * 3 * w + k * w) * 4
            px[b:b + w * 4] = src[a:a + w * 4]
    out.pixels = px
    out.filepath_raw = path
    out.file_format = 'PNG'
    out.save()
    for p in shots:
        os.remove(p)


def prepare(spec, lengths, probe=False):
    aid = spec['id']
    folder = os.path.join(ART, aid)
    src = os.path.join(folder, 'source.glb')
    if not os.path.exists(src):
        print('SKETCHFAB PREPARE: %s: no %s (tools/sketchfab_fetch.py)' % (aid, src))
        return None
    key = spec['length_of']
    if key not in lengths:
        raise RuntimeError('%s: no length for %s in data/scenery/' % (aid, key))
    length_m = lengths[key]
    clear_scene()
    bpy.ops.import_scene.gltf(filepath=src)
    if spec.get('forward') is None or probe:
        # Not oriented yet: the source as imported (glTF's y-up already turned to Blender's z-up), its parts' names
        # and a render of it with its own axes, to read "forward", "up" and "drop" off.
        names = sorted('%s [%s]' % (o.name, o.type) for o in bpy.context.scene.objects)
        obj, _ = join_meshes(spec.get('drop', []))
        if spec.get('keep'):
            keep_parts(obj, spec['keep'])
        yaw = straighten(obj, spec)
        obj.data.update()
        bpy.context.view_layer.update()  # the bounds after the turn, not before
        dims = obj.dimensions
        size = max(dims)
        bb = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
        centre = sum(bb, Vector()) / 8.0
        obj.data.transform(Matrix.Translation(-centre))
        check_render(obj, os.path.join(folder, 'probe.png'), size)
        print('SKETCHFAB PROBE: ' + json.dumps({'id': aid, 'yaw_deg': yaw, 'dims': [round(d, 3) for d in dims], 'triangles': triangles(obj),
                                                'objects': names[:60], 'object_count': len(names)}))
        return None
    obj, dropped = join_meshes(spec.get('drop', []))
    obj.name = spec['mesh_name']
    kept_out = keep_parts(obj, spec['keep']) if spec.get('keep') else 0
    yaw = straighten(obj, spec)
    orient(obj, spec.get('forward', '+x'), spec.get('up', '+z'))
    rotor = bool(spec.get('main_rotor'))  # a helicopter: its length without the blades
    band = spec.get('blade_band', 0.0)
    blades = blade_free_x(obj, band)[2] if rotor else 0
    scale = place(obj, length_m, axis_from_nose=key.startswith('munition:'), without_blades=rotor, blade_band=band)
    gear = 0
    if spec.get('gear_clear_m') is not None:
        gear = drop_gear(obj, spec['gear_clear_m'], spec.get('gear_part_m', 0.25 * length_m))
        # A probe at the nose or the tail may go with the gear: placed again, to the game's length about the middle.
        scale *= place(obj, length_m, axis_from_nose=key.startswith('munition:'), without_blades=rotor, blade_band=band)
    mats_before, _ = merge_materials(obj)
    tris = decimate(obj, spec.get('max_tris', 60000))
    shrink_textures(spec.get('texture_px', 2048))
    # Clean normals after the joins and the decimate.
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    out = os.path.join(folder, spec['mesh_name'] + '.glb')
    bpy.ops.export_scene.gltf(filepath=out, export_format='GLB', use_selection=True, export_apply=True,
                              export_yup=True, export_texcoords=True, export_normals=True, export_materials='EXPORT')
    obj.data.update()
    bpy.context.view_layer.update()
    dims = obj.dimensions
    check_render(obj, os.path.join(folder, 'check.png'), length_m)
    result = {'id': aid, 'out': out, 'length_m': length_m, 'scale_from_source': scale, 'yaw_deg': yaw, 'kept_out_vertices': kept_out, 'blade_parts': blades, 'gear_vertices': gear,'triangles': tris,
              'size_m': [round(dims.x, 3), round(dims.y, 3), round(dims.z, 3)], 'materials': len(obj.data.materials), 'materials_in_file': mats_before,
              'dropped': dropped}
    print('SKETCHFAB PREPARE: ' + json.dumps(result))
    return result


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    with open(MANIFEST, encoding='utf-8') as f:
        specs = json.load(f)['models']
    lengths = game_lengths()
    wanted = [s for s in specs if '--all' in argv or s['id'] in argv]
    for spec in wanted:
        try:
            prepare(spec, lengths, probe='--probe' in argv)
        except Exception as e:  # noqa: BLE001 - report and go on with the next model
            print('SKETCHFAB PREPARE: %s FAILED: %s' % (spec['id'], e))


main()
