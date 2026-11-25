"""One Megascans plant level to a GLB with an RGBA texture (tools/megascans_plant.py): imports the FBX, joins its meshes,
gives them one material of the texture (colour, alpha), and with --cross adds a copy turned 90 degrees (a billboard made
crossed cards). Prints a PLANT line with the triangles.

    blender --background --python tools/blender/megascans_plant.py -- IN.fbx TEXTURE.png OUT.glb [--cross]
"""
import json
import math
import sys

import bpy


def main():
    argv = sys.argv[sys.argv.index("--") + 1:]
    src, tex, out = argv[0], argv[1], argv[2]
    cross = "--cross" in argv
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=src)
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    bpy.ops.object.select_all(action="DESELECT")
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    if cross:
        dup = obj.copy()
        dup.data = obj.data.copy()
        bpy.context.scene.collection.objects.link(dup)
        dup.rotation_euler[2] = math.radians(90.0)
        bpy.ops.object.select_all(action="DESELECT")
        dup.select_set(True)
        bpy.context.view_layer.objects.active = dup
        bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.join()
    mat = bpy.data.materials.new("Foliage")
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    bsdf = nodes.get("Principled BSDF")
    img = nodes.new("ShaderNodeTexImage")
    img.image = bpy.data.images.load(tex)
    links.new(img.outputs["Color"], bsdf.inputs["Base Color"])
    links.new(img.outputs["Alpha"], bsdf.inputs["Alpha"])
    obj.data.materials.clear()
    obj.data.materials.append(mat)
    for o in list(bpy.context.scene.objects):
        if o is not obj:
            bpy.data.objects.remove(o, do_unlink=True)
    obj.data.calc_loop_triangles()
    tris = len(obj.data.loop_triangles)
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", use_selection=True)
    print("PLANT " + json.dumps({"out": out, "tris": tris}))


main()
