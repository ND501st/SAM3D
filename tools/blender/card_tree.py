"""A card-based tree or shrub for the map's near rings: a tapered trunk, a few branches, and many twig cards (quads with
a leaf photograph on alpha, from tools/leaf_atlas.py) over the crown. About 400-600 triangles for the near level, a third
of that from afar. One RGBA material, so it takes the foliage material and the season's tint like any model.

    blender --background --python tools/blender/card_tree.py -- SPECIES ATLAS.png OUT.glb [--lod 0|1]

The shape is a few numbers per species (SPECIES below): the trunk's share of the height, the crown's radius and profile
(its width by height), the branches and the cards. Seeded by the species, so a run is repeatable. The tree is one unit
tall; tools/prepare_model.py fits it to the frame. Atlas layout: 2 x 2 tiles, three twig cards and the bark.
"""
import json
import math
import random
import sys

import bpy
import bmesh
from mathutils import Matrix, Vector

# trunk: share of the height clear of leaves; r: crown radius (of the height); profile: crown width by height 0..1 of the
# crown; branches; cards; card: a card's side (of the height); stems: shrubs grow several stems from the ground.
SPECIES = {
    "hornbeam": dict(trunk=0.30, r=0.30, profile=[0.55, 0.9, 1.0, 0.8, 0.35], branches=9, cards=170, card=0.26, stems=1),
    "maple": dict(trunk=0.30, r=0.34, profile=[0.6, 0.95, 1.0, 0.85, 0.4], branches=9, cards=170, card=0.27, stems=1),
    "poplar": dict(trunk=0.16, r=0.13, profile=[0.55, 0.9, 1.0, 0.85, 0.4], branches=10, cards=180, card=0.2, stems=1),
    "hazel": dict(trunk=0.04, r=0.42, profile=[0.7, 1.0, 0.95, 0.7, 0.3], branches=0, cards=120, card=0.34, stems=5),
    "alder": dict(trunk=0.28, r=0.26, profile=[0.6, 0.95, 0.9, 0.7, 0.3], branches=8, cards=150, card=0.26, stems=1),
    "beech": dict(trunk=0.32, r=0.33, profile=[0.6, 0.95, 1.0, 0.85, 0.4], branches=9, cards=170, card=0.27, stems=1),
}
TILES = [(0, 0), (1, 0), (0, 1)]  # (column, row from the top) of the three twig cards; the bark is (1, 1)


def profile_at(p, t):
    t = min(max(t, 0.0), 1.0) * (len(p) - 1)
    i = min(int(t), len(p) - 2)
    return p[i] + (p[i + 1] - p[i]) * (t - i)


def tube(bm, a, b, ra, rb, sides, uv_layer):
    """A tapered tube from a to b, its UVs in the bark tile (bottom right of the atlas)."""
    axis = (b - a).normalized()
    helper = Vector((0, 0, 1)) if abs(axis.z) < 0.9 else Vector((1, 0, 0))
    u = axis.cross(helper).normalized()
    v = axis.cross(u).normalized()
    ring_a, ring_b = [], []
    for k in range(sides):
        ang = 2 * math.pi * k / sides
        d = u * math.cos(ang) + v * math.sin(ang)
        ring_a.append(bm.verts.new(a + d * ra))
        ring_b.append(bm.verts.new(b + d * rb))
    for k in range(sides):
        k2 = (k + 1) % sides
        f = bm.faces.new((ring_a[k], ring_a[k2], ring_b[k2], ring_b[k]))
        for loop, (uu, vv) in zip(f.loops, ((0.55, 0.05), (0.6, 0.05), (0.6, 0.4), (0.55, 0.4))):
            loop[uv_layer].uv = (uu, vv)


def card(bm, uv_layer, centre, up, roll, size, tile):
    """A twig card: a square whose base is at `centre`, its stem along `up`, turned by `roll` about it, with a twig tile."""
    up = up.normalized()
    helper = Vector((0, 0, 1)) if abs(up.z) < 0.9 else Vector((1, 0, 0))
    side = up.cross(helper).normalized()
    side = Matrix.Rotation(roll, 3, up) @ side
    h = size
    w = size * 0.3
    p0, p1 = centre - side * w, centre + side * w
    quad = [bm.verts.new(p0), bm.verts.new(p1), bm.verts.new(p1 + up * h), bm.verts.new(p0 + up * h)]
    f = bm.faces.new(quad)
    col, row = tile
    u0, u1 = col * 0.5 + 0.25 * 0.5 * 0.0, col * 0.5 + 0.5
    v1 = 1.0 - row * 0.5
    v0 = v1 - 0.5
    # the twig fills the middle of its tile; the quad takes that middle so little transparent area is drawn
    ua, ub = u0 + 0.5 * 0.2, u1 - 0.5 * 0.2
    for loop, (uu, vv) in zip(f.loops, ((ua, v0), (ub, v0), (ub, v1), (ua, v1))):
        loop[uv_layer].uv = (uu, vv)


def build(species, lod):
    prm = SPECIES[species]
    rng = random.Random(species + str(lod))
    mesh = bpy.data.meshes.new(species)
    bm = bmesh.new()
    uv = bm.loops.layers.uv.new("UVMap")
    crown_lo = prm["trunk"]
    crown_h = 1.0 - crown_lo
    stems = prm["stems"]
    tops = []
    for s in range(stems):
        ang = 2 * math.pi * s / max(1, stems) + rng.uniform(-0.3, 0.3)
        lean = 0.0 if stems == 1 else 0.28 * prm["r"]
        base = Vector((0, 0, 0))
        top = Vector((math.cos(ang) * lean, math.sin(ang) * lean, 0.55 + 0.4 * (1 if stems == 1 else rng.uniform(0.6, 1.0))))
        if stems == 1:
            top = Vector((0, 0, 0.9))
        tube(bm, base, top, 0.022 if stems == 1 else 0.012, 0.006, 6 if lod == 0 else 4, uv)
        tops.append(top)
    # branches: out and up from the trunk through the crown
    nb = prm["branches"] if lod == 0 else max(0, prm["branches"] // 3)
    for k in range(nb):
        t = (k + 0.5) / nb
        z0 = crown_lo + 0.55 * crown_h * t
        az = k * 2.399963 + rng.uniform(-0.2, 0.2)
        reach = prm["r"] * profile_at(prm["profile"], (z0 - crown_lo) / crown_h) * 0.9
        a = Vector((0, 0, z0))
        b = Vector((math.cos(az) * reach, math.sin(az) * reach, z0 + reach * 0.55))
        tube(bm, a, b, 0.009, 0.003, 4, uv)
    # cards over the crown envelope, their stems pointing out and up
    n = prm["cards"] if lod == 0 else int(prm["cards"] * 0.4)
    size = prm["card"] * (1.0 if lod == 0 else 1.5)
    for k in range(n):
        t = rng.random() ** 0.8
        z = crown_lo + crown_h * (0.08 + 0.9 * t)
        rr = prm["r"] * profile_at(prm["profile"], (z - crown_lo) / crown_h)
        rad = rr * math.sqrt(rng.random()) * 0.95 + 0.05 * rr
        az = rng.uniform(0, 2 * math.pi)
        c = Vector((math.cos(az) * rad, math.sin(az) * rad, z))
        if stems > 1:
            c = c + rng.choice(tops).xy.to_3d() * 0.0
        out = Vector((math.cos(az), math.sin(az), 0.0))
        up = (out * rng.uniform(0.4, 1.0) + Vector((0, 0, rng.uniform(0.15, 0.8)))).normalized()
        centre = c - up * (size * 0.4)
        card(bm, uv, centre, up, rng.uniform(0, math.pi), size * rng.uniform(0.85, 1.15), TILES[rng.randrange(3)])
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(species, mesh)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def main():
    argv = sys.argv[sys.argv.index("--") + 1:]
    species, atlas, out = argv[0], argv[1], argv[2]
    lod = int(argv[argv.index("--lod") + 1]) if "--lod" in argv else 0
    bpy.ops.wm.read_factory_settings(use_empty=True)
    obj = build(species, lod)
    mat = bpy.data.materials.new("Foliage")
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    bsdf = nodes.get("Principled BSDF")
    img = nodes.new("ShaderNodeTexImage")
    img.image = bpy.data.images.load(atlas)
    links.new(img.outputs["Color"], bsdf.inputs["Base Color"])
    links.new(img.outputs["Alpha"], bsdf.inputs["Alpha"])
    obj.data.materials.append(mat)
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    obj.data.calc_loop_triangles()
    tris = len(obj.data.loop_triangles)
    bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", use_selection=True, export_yup=True)
    print("CARDTREE " + json.dumps({"species": species, "lod": lod, "tris": tris, "out": out}))


main()
