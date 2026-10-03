"""Build the cabin's console parts in Blender from code and export them for Unreal.

Every part is modelled at 1 m across (its diameter, or width) with real proportions, standing out of the panel along
+Z from its base at Z = 0, and "up the face" along +Y. The cabin (unreal/.../SimCabinComponent) scales each instance by
the item's size from data/cabins/<system>.json and tints it with the item's colours, so one mesh serves every size.
Moving parts are separate meshes: toggle levers, knob bodies, button caps, lamp lenses, wheel handles.

    "C:/Program Files/Blender Foundation/Blender 4.2/blender.exe" --background --python tools/blender/build_cabin_parts.py
    ... -- --preview out/parts_preview.png      # also render a contact sheet

Writes art/cabin_parts/<Part>.fbx (one mesh each) and art/cabin_parts/cabin_parts.blend (Git LFS).
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(REPO, 'art', 'cabin_parts')
SEG = 48  # segments around round parts


def clear():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def new_object(name, bm):
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    for p in mesh.polygons:
        p.use_smooth = True
    return obj


def lathe(bm, profile, seg=SEG, ribs=0, rib_depth=0.0, rib_rows=()):
    """Revolve a profile [(r, z), ...] (from the axis out and back) around Z. Rows listed in rib_rows get `ribs`
    knurling ridges (radius modulated by rib_depth). Returns the new vertices row by row."""
    rows = []
    for i, (r, z) in enumerate(profile):
        row = []
        for k in range(seg):
            a = 2 * math.pi * k / seg
            rr = r
            if ribs and i in rib_rows:
                rr = r * (1.0 - rib_depth * (0.5 + 0.5 * math.cos(ribs * a)))
            row.append(bm.verts.new((rr * math.cos(a), rr * math.sin(a), z)))
        rows.append(row)
    for i in range(len(rows) - 1):
        for k in range(seg):
            a, b = rows[i][k], rows[i][(k + 1) % seg]
            c, d = rows[i + 1][(k + 1) % seg], rows[i + 1][k]
            bm.faces.new((a, b, c, d))
    # Close the ends: a fan to a centre vertex where the profile touches the axis.
    for row, z, flip in ((rows[0], profile[0][1], True), (rows[-1], profile[-1][1], False)):
        if profile[0 if flip else -1][0] < 1e-6:
            continue
        centre = bm.verts.new((0.0, 0.0, z))
        for k in range(seg):
            a, b = row[k], row[(k + 1) % seg]
            bm.faces.new((centre, b, a) if flip else (centre, a, b))
    return rows


def box(bm, cx, cy, cz, sx, sy, sz):
    m = Matrix.Translation((cx, cy, cz)) @ Matrix.Diagonal((sx, sy, sz, 1.0))
    bmesh.ops.create_cube(bm, size=1.0, matrix=m)


def cylinder(bm, r, z0, z1, x=0.0, y=0.0, seg=SEG):
    m = Matrix.Translation((x, y, (z0 + z1) / 2))
    bmesh.ops.create_cone(bm, cap_ends=True, segments=seg, radius1=r, radius2=r, depth=z1 - z0, matrix=m)


def bevel_all(obj, width=0.02, segments=2, angle=35.0):
    mod = obj.modifiers.new('bevel', 'BEVEL')
    mod.width = width
    mod.segments = segments
    mod.limit_method = 'ANGLE'
    mod.angle_limit = math.radians(angle)
    mod = obj.modifiers.new('normals', 'WEIGHTED_NORMAL')
    mod.keep_sharp = True


def part(name, build, bevel=0.015):
    bm = bmesh.new()
    build(bm)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-6)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    obj = new_object(name, bm)
    if bevel:
        bevel_all(obj, bevel)
    return obj


# ---------------------------------------------------------------------------------------------- the parts

def knob(bm):
    # Bakelite knob: a ribbed skirt, a slightly tapered ribbed body, a rounded top.
    lathe(bm, [(0.0, 0.0), (0.5, 0.0), (0.5, 0.12), (0.43, 0.15), (0.42, 0.52), (0.36, 0.6), (0.0, 0.62)],
          ribs=36, rib_depth=0.04, rib_rows=(1, 2, 3, 4))


def knob_pointer(bm):
    knob(bm)
    box(bm, 0.0, 0.2, 0.635, 0.07, 0.36, 0.04)    # raised pointer bar up the face


def trim(bm):
    # Slotted trimmer head in its collar.
    lathe(bm, [(0.0, 0.0), (0.5, 0.0), (0.5, 0.1), (0.36, 0.1), (0.36, 0.22), (0.3, 0.26), (0.0, 0.26)])
    box(bm, 0.0, 0.0, 0.27, 0.62, 0.1, 0.06)       # the slot's raised lips stand in for the cut


def toggle_base(bm):
    # Hex nut on a threaded bushing.
    m = Matrix.Translation((0, 0, 0.08))
    bmesh.ops.create_cone(bm, cap_ends=True, segments=6, radius1=0.5, radius2=0.5, depth=0.16, matrix=m)
    cylinder(bm, 0.27, 0.16, 0.42)


def toggle_lever(bm):
    # 1 m long from the pivot: a tapered rod and a ball tip.
    lathe(bm, [(0.0, 0.0), (0.11, 0.0), (0.085, 0.86), (0.0, 0.86)], seg=24)
    bmesh.ops.create_uvsphere(bm, u_segments=24, v_segments=12, radius=0.13, matrix=Matrix.Translation((0, 0, 0.9)))


def lamp_bezel(bm):
    # Chrome bezel ring around the lens.
    lathe(bm, [(0.34, 0.0), (0.5, 0.0), (0.5, 0.12), (0.46, 0.2), (0.36, 0.2), (0.34, 0.14)])


def lamp_lens(bm):
    # The glass dome (tinted and lit by the cabin).
    lathe(bm, [(0.0, 0.0), (0.36, 0.0), (0.36, 0.14), (0.3, 0.26), (0.17, 0.32), (0.0, 0.34)])


def button_ring(bm):
    lathe(bm, [(0.36, 0.0), (0.5, 0.0), (0.5, 0.14), (0.46, 0.22), (0.38, 0.22), (0.36, 0.16)])


def button_cap(bm):
    lathe(bm, [(0.0, 0.0), (0.34, 0.0), (0.34, 0.26), (0.3, 0.33), (0.0, 0.35)])


def jack(bm):
    # Insulated test jack: a collar, the coloured insulator, the hole.
    lathe(bm, [(0.0, 0.0), (0.5, 0.0), (0.5, 0.1), (0.42, 0.12), (0.4, 0.55), (0.35, 0.6), (0.16, 0.6),
               (0.16, 0.45), (0.0, 0.45)])


def socket(bm):
    # Coaxial connector: knurled body, the dark insulator, the centre pin.
    lathe(bm, [(0.0, 0.0), (0.5, 0.0), (0.5, 0.45), (0.44, 0.5), (0.3, 0.5), (0.3, 0.4), (0.06, 0.4),
               (0.06, 0.55), (0.0, 0.55)], ribs=30, rib_depth=0.05, rib_rows=(1, 2))


def latch(bm):
    # Block lock: a stadium-shaped plate and the slotted barrel (ОТКР. / ЗАКР.).
    seg = 24
    pts = []
    for k in range(seg + 1):
        a = math.pi * k / seg
        pts.append((0.32 * math.cos(a), 0.38 + 0.32 * math.sin(a)))
    for k in range(seg + 1):
        a = math.pi + math.pi * k / seg
        pts.append((0.32 * math.cos(a), -0.38 + 0.32 * math.sin(a)))
    bottom = [bm.verts.new((x, y, 0.0)) for x, y in pts]
    top = [bm.verts.new((x, y, 0.08)) for x, y in pts]
    bm.faces.new(list(reversed(bottom)))
    bm.faces.new(top)
    n = len(pts)
    for k in range(n):
        bm.faces.new((bottom[k], bottom[(k + 1) % n], top[(k + 1) % n], top[k]))
    lathe(bm, [(0.0, 0.08), (0.24, 0.08), (0.24, 0.42), (0.2, 0.46), (0.0, 0.46)])
    box(bm, 0.0, 0.0, 0.47, 0.06, 0.34, 0.04)


def wheel_dish(bm):
    # The hand wheel: a shallow dish with a rounded rim, on a hub.
    lathe(bm, [(0.0, 0.0), (0.12, 0.0), (0.12, 0.06), (0.4, 0.1), (0.47, 0.16), (0.5, 0.22), (0.48, 0.26),
               (0.44, 0.24), (0.38, 0.17), (0.12, 0.13), (0.0, 0.13)])


def wheel_handle(bm):
    # The crank handle: 1 m long, a stem and a spinner grip.
    lathe(bm, [(0.0, 0.0), (0.12, 0.0), (0.12, 0.3), (0.2, 0.34), (0.22, 0.9), (0.16, 1.0), (0.0, 1.0)], seg=24)


def screw(bm):
    # Cheese-head panel screw with a slot.
    lathe(bm, [(0.0, 0.0), (0.5, 0.0), (0.5, 0.2), (0.42, 0.3), (0.0, 0.3)], seg=24)
    box(bm, 0.0, 0.0, 0.31, 0.8, 0.12, 0.03)


PARTS = [
    ('SM_Knob', knob, 0.012), ('SM_KnobPointer', knob_pointer, 0.012), ('SM_Trim', trim, 0.01),
    ('SM_ToggleBase', toggle_base, 0.015), ('SM_ToggleLever', toggle_lever, 0.0), ('SM_LampBezel', lamp_bezel, 0.01),
    ('SM_LampLens', lamp_lens, 0.0), ('SM_ButtonRing', button_ring, 0.01), ('SM_ButtonCap', button_cap, 0.0),
    ('SM_Jack', jack, 0.01), ('SM_Socket', socket, 0.01), ('SM_Latch', latch, 0.01), ('SM_WheelDish', wheel_dish, 0.0),
    ('SM_WheelHandle', wheel_handle, 0.0), ('SM_Screw', screw, 0.0),
]


def export(obj):
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    path = os.path.join(OUT, obj.name + '.fbx')
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, apply_scale_options='FBX_SCALE_ALL',
                             axis_forward='-Y', axis_up='Z', use_mesh_modifiers=True, mesh_smooth_type='FACE',
                             add_leaf_bones=False, bake_anim=False)
    return path


def preview(objs, path):
    """A contact sheet: the parts in a row of rows, lit and rendered from in front and above."""
    cols = 5
    for i, obj in enumerate(objs):
        obj.location = Vector(((i % cols) * 1.5, -(i // cols) * 1.5, 0.0))
    mat = bpy.data.materials.new('preview')
    mat.use_nodes = True
    mat.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (0.55, 0.55, 0.52, 1)
    mat.node_tree.nodes['Principled BSDF'].inputs['Roughness'].default_value = 0.35
    for obj in objs:
        obj.data.materials.append(mat)
    rows = (len(objs) + cols - 1) // cols
    centre = Vector(((cols - 1) * 0.75, -(rows - 1) * 0.75, 0.3))
    cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam'))
    bpy.context.scene.collection.objects.link(cam)
    cam.location = centre + Vector((0.0, -6.0, 7.0))
    cam.rotation_euler = (centre - cam.location).to_track_quat('-Z', 'Y').to_euler()
    cam.data.lens = 38
    bpy.context.scene.camera = cam
    for loc, energy in (((4, -6, 8), 900), ((-4, 2, 6), 400)):
        light = bpy.data.objects.new('light', bpy.data.lights.new('light', 'AREA'))
        light.data.energy = energy
        light.data.size = 5
        light.location = centre + Vector(loc)
        light.rotation_euler = (centre - light.location).to_track_quat('-Z', 'Y').to_euler()
        bpy.context.scene.collection.objects.link(light)
    world = bpy.data.worlds.new('w')
    world.use_nodes = True
    world.node_tree.nodes['Background'].inputs['Color'].default_value = (0.05, 0.05, 0.06, 1)
    bpy.context.scene.world = world
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_EEVEE_NEXT' if 'BLENDER_EEVEE_NEXT' in bpy.types.RenderSettings.bl_rna.properties['engine'].enum_items.keys() else 'BLENDER_EEVEE'
    sc.render.resolution_x, sc.render.resolution_y = 1400, 900
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    preview_path = argv[argv.index('--preview') + 1] if '--preview' in argv else None
    clear()
    os.makedirs(OUT, exist_ok=True)
    objs = [part(name, build, bev) for name, build, bev in PARTS]
    for obj in objs:
        export(obj)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, 'cabin_parts.blend'))
    print('exported', len(objs), 'parts to', OUT)
    if preview_path:
        preview(objs, os.path.abspath(preview_path))
        print('preview', preview_path)


main()
