"""Render the crew's bodies posed in the cabin, to check by eye that each crewman sits in his seat and his hands meet the
controls (roadmap 8.10).

simcore poses the bodies (simcore/crew_body.hpp); simcore/examples/crew_pose_dump writes the poses as JSON. This script
furnishes the compartment from data/cabins/osa_ak.json (the interior meshes of art/cabin_interior at their places, the
console panels as plates), puts the body parts of art/crew_body (tools/blender/build_crew_body.py) where each pose says,
marks the control being reached with a red bead, and renders one picture per pose.

    simcore/build/Release/crew_pose_dump.exe data out/crew_poses.json
    "C:/Program Files/Blender Foundation/Blender 4.2/blender.exe" --background --python tools/blender/render_crew_poses.py \
        -- --poses out/crew_poses.json --out out/crew_poses [--only deputy_orient]
"""
import json
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
LAYOUT = os.path.join(REPO, 'data', 'cabins', 'osa_ak.json')
INTERIOR = os.path.join(REPO, 'art', 'cabin_interior')
CREW = os.path.join(REPO, 'art', 'crew_body', 'crew_body.blend')
SKIP = {'SM_OsaHull', 'SM_OsaGlass'}  # the shell would hide the bodies from the cameras
COLOURS = {'M_CrewCloth': (0.20, 0.22, 0.13, 1), 'M_CrewSkin': (0.62, 0.45, 0.36, 1), 'M_CrewHelmet': (0.03, 0.03, 0.03, 1),
           'M_CrewBoot': (0.02, 0.02, 0.02, 1), 'M_CrewBelt': (0.25, 0.14, 0.07, 1)}


def P(v):
    """Body frame (x right, y forward, z up) to Blender (X forward, Y left, Z up)."""
    return Vector((v[1], -v[0], v[2]))


def material(name, colour, rough=0.7):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = m.node_tree.nodes.get('Principled BSDF')
    if bsdf is not None:
        bsdf.inputs['Base Color'].default_value = colour
        bsdf.inputs['Roughness'].default_value = rough
    return m


def furnish(layout):
    for part in layout['shell']:
        if part.get('kind') != 'mesh' or part['mesh'] in SKIP:
            continue
        path = os.path.join(INTERIOR, part['mesh'] + '.fbx')
        if not os.path.exists(path):
            continue
        before = set(bpy.data.objects)
        bpy.ops.import_scene.fbx(filepath=path, axis_forward='-Y', axis_up='Z')
        for o in set(bpy.data.objects) - before:
            o.location = P(part['centre_m'])
            o.rotation_euler = (0.0, 0.0, -math.radians(part.get('yaw_deg', 0.0)))
            for slot in o.material_slots:
                if slot.material is not None:
                    slot.material = material(slot.material.name, (0.32, 0.36, 0.33, 1))
    plate = material('panel_plate', (0.55, 0.54, 0.47, 1))
    for p in layout['panels']:
        f, t = math.radians(p['facing_deg']), math.radians(p['tilt_deg'])
        n = Vector((math.sin(f) * math.cos(t), math.cos(f) * math.cos(t), math.sin(t)))
        r = Vector((-math.cos(f), math.sin(f), 0.0))
        u = n.cross(r)
        w, h = p['size_m']
        bpy.ops.mesh.primitive_cube_add(size=1.0)
        o = bpy.context.active_object
        o.name = 'panel_' + p['id']
        # the plate's axes in Blender: across = right, up the face = up, depth = normal
        rb, ub, nb = P(r), P(u), P(n)
        o.matrix_world = Matrix((
            (rb.x * w, ub.x * h, nb.x * 0.01, 0.0),
            (rb.y * w, ub.y * h, nb.y * 0.01, 0.0),
            (rb.z * w, ub.z * h, nb.z * 0.01, 0.0),
            (0.0, 0.0, 0.0, 1.0)))
        o.location = P(p['centre_m']) - nb * 0.005
        o.data.materials.append(plate)


def load_crew():
    with bpy.data.libraries.load(CREW, link=False) as (src, dst):
        dst.objects = [n for n in src.objects if n.startswith('SM_Crew_')]
    templates = {}
    for o in dst.objects:
        if o is None:
            continue
        for slot in o.material_slots:
            if slot.material is not None:
                key = slot.material.name.split('.')[0]
                slot.material = material(key + '_r', COLOURS.get(key, (0.5, 0.5, 0.5, 1)))
        templates[o.name] = o
    return templates


def mesh_for(part):
    n = part['name']
    base = {'pelvis': 'Pelvis', 'torso': 'Torso', 'neck': 'Neck', 'head': 'Head', 'upper_arm': 'UpperArm', 'forearm': 'Forearm',
            'thigh': 'Thigh', 'shin': 'Shin', 'foot': 'Foot'}
    for k, v in base.items():
        if n == k or n.startswith(k + '_'):
            return 'SM_Crew_' + v
    if n.startswith('hand_'):
        shape = {'rest': 'Rest', 'point': 'Point', 'grip': 'Grip'}[part['hand']]
        return 'SM_Crew_Hand%s_%s' % (shape, 'L' if n.endswith('_l') else 'R')
    return None


def place_pose(pose, templates):
    made = []
    for part in pose['parts']:
        t = templates.get(mesh_for(part))
        if t is None:
            continue
        o = bpy.data.objects.new(part['name'], t.data)
        bpy.context.scene.collection.objects.link(o)
        x = P(part['bone']).normalized()
        z = P(part['up']).normalized()
        y = z.cross(x).normalized()
        z = x.cross(y).normalized()
        o.matrix_world = Matrix(((x.x, y.x, z.x, 0.0), (x.y, y.y, z.y, 0.0), (x.z, y.z, z.z, 0.0), (0.0, 0.0, 0.0, 1.0)))
        o.location = P(part['origin'])
        made.append(o)
    if 'touch' in pose:
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.012, location=P(pose['touch']))
        bead = bpy.context.active_object
        bead.data.materials.append(material('bead', (0.9, 0.05, 0.02, 1), 0.3))
        made.append(bead)
    return made


def camera_for(pose, cam):
    """A camera behind or beside him with a clear view (ray-cast against the furniture before the body is placed),
    inside the compartment."""
    pelvis = next(p for p in pose['parts'] if p['name'] == 'pelvis')
    fwd = Vector(pelvis['up'])
    fwd.z = 0.0
    fwd.normalize()
    right = Vector((fwd.y, -fwd.x, 0.0))
    up = Vector((0.0, 0.0, 1.0))
    hip = Vector(pose['hip'])
    look = hip + up * 0.35 + fwd * 0.15
    if 'touch' in pose:
        look = (look + Vector(pose['touch'])) * 0.5
    sc = bpy.context.scene
    dg = bpy.context.evaluated_depsgraph_get()
    chosen = None
    for ang in (-140, 140, -110, 110, -80, 80, 180, -50, 50):
        a = math.radians(ang)
        d = fwd * math.cos(a) + right * math.sin(a)
        for dist in (1.1, 0.85, 0.6):
            eye = look + d * dist + up * 0.3
            if not (abs(eye.x) < 1.15 and -0.85 < eye.y < 4.75 and 0.8 < eye.z < 2.05):
                continue
            o, e = P(look), P(eye)
            hit = sc.ray_cast(dg, o, (e - o).normalized(), distance=(e - o).length)[0]
            if not hit:
                chosen = eye
                break
        if chosen is not None:
            break
    if chosen is None:
        chosen = look - fwd * 0.6 + up * 0.4
    cam.location = P(chosen)
    dvec = (P(look) - P(chosen)).normalized()
    cam.rotation_euler = dvec.to_track_quat('-Z', 'Y').to_euler()


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    poses_path = os.path.abspath(argv[argv.index('--poses') + 1])
    out = os.path.abspath(argv[argv.index('--out') + 1])
    only = argv[argv.index('--only') + 1] if '--only' in argv else None
    bpy.ops.wm.read_factory_settings(use_empty=True)
    os.makedirs(out, exist_ok=True)
    with open(LAYOUT, encoding='utf-8') as f:
        layout = json.load(f)
    with open(poses_path, encoding='utf-8') as f:
        poses = json.load(f)['poses']
    furnish(layout)
    templates = load_crew()
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_EEVEE_NEXT' if 'BLENDER_EEVEE_NEXT' in bpy.types.RenderSettings.bl_rna.properties['engine'].enum_items.keys() else 'BLENDER_EEVEE'
    sc.render.resolution_x, sc.render.resolution_y = 960, 640
    world = bpy.data.worlds.new('w')
    world.use_nodes = True
    world.node_tree.nodes['Background'].inputs['Color'].default_value = (0.5, 0.52, 0.55, 1)
    world.node_tree.nodes['Background'].inputs['Strength'].default_value = 1.2
    sc.world = world
    for y in (0.6, 2.0, 3.4):
        lamp = bpy.data.objects.new('lamp', bpy.data.lights.new('lamp', 'POINT'))
        lamp.data.energy = 60
        lamp.location = P((0.1, y, 2.0))
        sc.collection.objects.link(lamp)
    cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam'))
    sc.collection.objects.link(cam)
    cam.data.lens = 16
    cam.data.clip_start = 0.02
    sc.camera = cam
    for pose in poses:
        if only and pose['name'] != only:
            continue
        camera_for(pose, cam)  # before the body: it mustn't block its own camera
        made = place_pose(pose, templates)
        sc.render.filepath = os.path.join(out, pose['name'] + '.png')
        bpy.ops.render.render(write_still=True)
        for o in made:
            bpy.data.objects.remove(o, do_unlink=True)
        print('rendered', pose['name'])


if __name__ == '__main__':
    main()
