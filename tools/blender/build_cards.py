"""Builds the ground clutter's card meshes (tools/make_cards.py): a few crossed quads, each a unit wide and a unit tall with its
base at z = 0, UV over the whole texture, carrying the card's alpha texture. One GLB per card next to its PNG.

    blender --background --python tools/blender/build_cards.py -- art/clutter
"""
import math
import sys
from pathlib import Path

import bpy

# name: (quads, turn between them in degrees): three quads 60 degrees apart round a tuft, two crossed for the thinner plants.
CARDS = {"grass_card": (3, 60.0), "straw_card": (2, 90.0), "reed_card": (2, 90.0), "maize_card": (2, 90.0), "maize_card_b": (2, 90.0),
         "maize_card_c": (2, 90.0), "maize_card_d": (2, 90.0), "maize_card_e": (2, 90.0), "maize_card_f": (2, 90.0),
         "sunflower_card": (2, 90.0), "sunflower_card_b": (2, 90.0), "sunflower_card_c": (2, 90.0)}


def main():
    out = Path(sys.argv[sys.argv.index("--") + 1])
    for name, (quads, turn) in CARDS.items():
        bpy.ops.wm.read_factory_settings(use_empty=True)
        verts, faces, uvs = [], [], []
        for q in range(quads):
            a = math.radians(turn * q)
            dx, dy = math.cos(a) * 0.5, math.sin(a) * 0.5
            base = len(verts)
            verts += [(-dx, -dy, 0.0), (dx, dy, 0.0), (dx, dy, 1.0), (-dx, -dy, 1.0)]
            faces.append((base, base + 1, base + 2, base + 3))
            uvs += [(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)]
        mesh = bpy.data.meshes.new(name)
        mesh.from_pydata(verts, [], faces)
        mesh.update()
        uv = mesh.uv_layers.new(name="UVMap")
        for loop in mesh.loops:
            uv.data[loop.index].uv = uvs[loop.vertex_index]
        obj = bpy.data.objects.new(name, mesh)
        bpy.context.scene.collection.objects.link(obj)
        mat = bpy.data.materials.new(name)
        mat.use_nodes = True
        nodes = mat.node_tree.nodes
        links = mat.node_tree.links
        bsdf = nodes.get("Principled BSDF")
        tex = nodes.new("ShaderNodeTexImage")
        tex.image = bpy.data.images.load(str(out / (name + ".png")))
        links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
        links.new(tex.outputs["Alpha"], bsdf.inputs["Alpha"])
        try:
            mat.surface_render_method = "DITHERED"
        except Exception:
            pass
        mesh.materials.append(mat)
        bpy.ops.object.select_all(action="DESELECT")
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        bpy.ops.export_scene.gltf(filepath=str(out / (name + ".glb")), export_format="GLB", use_selection=True)
        print("CARD " + name)


main()
