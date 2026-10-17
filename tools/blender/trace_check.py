"""Renders that check a traced model against its photographs (docs/MODELLING_FROM_PHOTOS.md), run inside Blender (over the
Blender MCP, or a background run). Their output goes to tools/photo_trace.py overlay.

    import trace_check as C
    C.render_front(obj_front, cfg, out_png)            # ortho, square on, at the flattened front's px/mm and frame
    C.render_photo_camera(obj_front, cfg, cam_json, out_png)   # from the photo's solved camera, transparent background
    C.render_views(target, out_prefix, views)          # three-quarter close-ups to look for parts running into each other

`obj_front` says where the traced front lies in Blender: {'x': the front plane's Blender X (the models face +X here: the
builders' body +y), 'y': its centre's Blender Y, 'z': its centre's Z}. The trace's x (right) is Blender +Y, its y (down) is
Blender -Z, into the object is -X. `cfg` is the tools/traces/<name>.json config (width_mm, height_mm, px_per_mm, margin_mm).
"""
import json
import math

import bpy
from mathutils import Matrix, Vector


def _scene_setup(res, transparent=False):
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_EEVEE'
    sc.render.resolution_x, sc.render.resolution_y = res
    sc.render.resolution_percentage = 100
    sc.render.film_transparent = transparent
    if sc.world is None:
        sc.world = bpy.data.worlds.new('trace_world')
    sc.world.color = (0.05, 0.05, 0.055)
    return sc


def _object(name, data):
    o = bpy.data.objects.get(name)
    if o is not None:
        bpy.data.objects.remove(o, do_unlink=True)
    o = bpy.data.objects.new(name, data)
    bpy.context.scene.collection.objects.link(o)
    return o


def lights(target):
    """A key from the upper left front and a fill from the right, aimed at target (Blender coords)."""
    t = Vector(target)
    for name, energy, off in (('trace_key', 60, (0.8, 0.35, 0.6)), ('trace_fill', 25, (0.8, -0.5, 0.1))):
        L = bpy.data.lights.get(name) or bpy.data.lights.new(name, 'AREA')
        L.energy = energy
        L.size = 0.8
        o = _object(name, L)
        o.location = t + Vector(off)
        o.rotation_euler = (t - o.location).to_track_quat('-Z', 'Y').to_euler()


def render_front(front, cfg, out_png):
    """Square on, orthographic, framed exactly as photo_trace's front_mm.png (the front plus its margin at px_per_mm)."""
    k, m = cfg.get('px_per_mm', 5.0), cfg.get('margin_mm', 40.0)
    W, H = cfg['width_mm'] + 2 * m, cfg['height_mm'] + 2 * m
    sc = _scene_setup((int(W * k), int(H * k)))
    cam = _object('trace_front', bpy.data.cameras.new('trace_front'))
    cam.data.type = 'ORTHO'
    cam.data.ortho_scale = max(W, H) / 1000.0
    cam.data.clip_start = 0.01
    cam.location = Vector((front['x'] + 1.0, front['y'], front['z']))
    cam.rotation_euler = (math.radians(90), 0, math.radians(90))
    sc.camera = cam
    lights(Vector((front['x'], front['y'], front['z'])))
    sc.render.filepath = out_png
    bpy.ops.render.render(write_still=True)
    return out_png


def render_photo_camera(front, cfg, cam_json, out_png, width=1500):
    """From the camera photo_trace solved for the photo (camera.json: R, t in the plate's frame, x right, y down, z into the
    object, origin at its top left), with a transparent background, at the photo's aspect."""
    c = json.load(open(cam_json))
    R = c['R']
    t = c['t']
    Rt = [[R[j][i] for j in range(3)] for i in range(3)]
    C = [-sum(Rt[i][j] * t[j] for j in range(3)) for i in range(3)]
    x0 = front['x']
    y0 = front['y'] - cfg['width_mm'] / 2000.0
    z0 = front['z'] + cfg['height_mm'] / 2000.0

    def p(v):
        return Vector((x0 - v[2], y0 + v[0], z0 - v[1]))

    def d(v):
        return Vector((-v[2], v[0], -v[1]))

    X, Y, Z = d(R[0]), -d(R[1]), -d(R[2])          # OpenCV camera (x right, y down, z ahead) -> Blender (y up, -z ahead)
    rot = Matrix((X, Y, Z)).transposed()
    sc = _scene_setup((width, int(width * c['h'] / c['w'])), transparent=True)
    cam = _object('trace_photo', bpy.data.cameras.new('trace_photo'))
    cam.matrix_world = Matrix.Translation(p(C)) @ rot.to_4x4()
    cam.data.sensor_fit = 'HORIZONTAL'
    cam.data.sensor_width = 36.0
    cam.data.lens = c['f_px'] / c['w'] * 36.0
    cam.data.clip_start = 0.05
    sc.camera = cam
    lights(Vector((front['x'], front['y'], front['z'])))
    sc.render.filepath = out_png
    bpy.ops.render.render(write_still=True)
    sc.render.film_transparent = False
    return out_png


def render_views(target, out_prefix, views, res=(1400, 1000), lens=50):
    """Perspective close-ups: views {name: (offset from target (Blender), )}; each looks at target."""
    sc = _scene_setup(res)
    cam = _object('trace_close', bpy.data.cameras.new('trace_close'))
    cam.data.lens = lens
    cam.data.clip_start = 0.01
    sc.camera = cam
    t = Vector(target)
    lights(t)
    out = []
    for name, off in views.items():
        cam.location = t + Vector(off)
        cam.rotation_euler = (t - cam.location).to_track_quat('-Z', 'Y').to_euler()
        sc.render.filepath = '%s_%s.png' % (out_prefix, name)
        bpy.ops.render.render(write_still=True)
        out.append(sc.render.filepath)
    return out
