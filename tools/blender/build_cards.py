"""Builds the ground clutter's card meshes (tools/make_cards.py): a few crossed quads, each a unit wide and a unit tall with its
base at z = 0, UV over the whole texture, carrying the card's alpha texture. One GLB per card next to its PNG.

    blender --background --python tools/blender/build_cards.py -- art/clutter [card ...]
"""
import math
import sys
from pathlib import Path

import bpy

# name: (quads, turn between them in degrees): three quads 60 degrees apart round a tuft, two crossed for the thinner plants.
CARDS = {"grass_card": (3, 60.0), "straw_card": (2, 90.0), "reed_card": (2, 90.0), "maize_card": (2, 90.0), "maize_card_b": (2, 90.0),
         "maize_card_c": (2, 90.0), "maize_card_d": (2, 90.0), "maize_card_e": (2, 90.0), "maize_card_f": (2, 90.0),
         "sunflower_card": (2, 90.0), "sunflower_card_b": (2, 90.0), "sunflower_card_c": (2, 90.0),
         "hazel_card": (3, 60.0), "elder_card": (3, 60.0),
         "poplar_card": (3, 60.0), "alder_card": (3, 60.0), "acacia_card": (3, 60.0), "maple_card": (3, 60.0), "pine_card": (3, 60.0), "beech_card": (3, 60.0),
         "willow_card": (3, 60.0)}

# A tree card's crown seen from above: (its height, its width, both of the unit), a flat quad carrying the right half of
# the card's texture (make_cards.py tree_card with top=True). Tried in A.3.24 and left out: from the ground the quad
# shows as a dark bar across the crown, and the player is mostly on the ground.
TOPS = {}


def main():
    args = sys.argv[sys.argv.index("--") + 1:]
    out = Path(args[0])
    only = set(args[1:])  # the cards to build, all of them when none is named
    for name, (quads, turn) in CARDS.items():
        if only and name not in only:
            continue
        bpy.ops.wm.read_factory_settings(use_empty=True)
        verts, faces, uvs = [], [], []
        for q in range(quads):
            a = math.radians(turn * q)
            dx, dy = math.cos(a) * 0.5, math.sin(a) * 0.5
            base = len(verts)
            verts += [(-dx, -dy, 0.0), (dx, dy, 0.0), (dx, dy, 1.0), (-dx, -dy, 1.0)]
            faces.append((base, base + 1, base + 2, base + 3))
            side_u = 0.5 if name in TOPS else 1.0
            uvs += [(0.0, 0.0), (side_u, 0.0), (side_u, 1.0), (0.0, 1.0)]
        if name in TOPS:
            z, size = TOPS[name]
            h = 0.5 * size
            base = len(verts)
            verts += [(-h, -h, z), (h, -h, z), (h, h, z), (-h, h, z)]
            faces.append((base, base + 1, base + 2, base + 3))
            uvs += [(0.5, 0.5), (1.0, 0.5), (1.0, 1.0), (0.5, 1.0)]
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
