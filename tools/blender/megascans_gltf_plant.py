"""One clump of a Megascans plant in Unreal's format (a Fab download unpacked: <id>_tier_2.gltf with every variant and LOD
as its own mesh, its maps as PNGs, Textures/T_<id>_2K_B-O.png the colour with the opacity as alpha) to a GLB with that
RGBA texture (tools/megascans_plant.py --gltf). Prints a PLANT line with the triangles and the clump's size.

    blender --background --python tools/blender/megascans_gltf_plant.py -- IN.gltf MESH_NAME TEXTURE.png OUT.glb
"""
import json
import sys

import bpy


def main():
    argv = sys.argv[sys.argv.index("--") + 1:]
    src, name, tex, out = argv[0], argv[1], argv[2], argv[3]
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=src)
    keep = [o for o in bpy.context.scene.objects if o.type == "MESH" and (o.name == name or o.data.name == name)]
    if not keep:
        names = sorted(o.name for o in bpy.context.scene.objects if o.type == "MESH")
        raise SystemExit("no mesh %s in %s (it has %s)" % (name, src, names))
    obj = keep[0]
    for o in list(bpy.context.scene.objects):
        if o is not obj:
            bpy.data.objects.remove(o, do_unlink=True)
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
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
    obj.data.calc_loop_triangles()
    tris = len(obj.data.loop_triangles)
    xs = [v.co.x for v in obj.data.vertices]
    ys = [v.co.y for v in obj.data.vertices]
    zs = [v.co.z for v in obj.data.vertices]
    size = [round(max(c) - min(c), 3) for c in (xs, ys, zs)]
    bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", use_selection=True)
    print("PLANT " + json.dumps({"out": out, "tris": tris, "size_m": size}))


main()
