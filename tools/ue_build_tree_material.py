"""Makes M_SimMapTree (roadmap T5.1, docs/scenery/T5_T8_PLAN.md): the map's vertex-colour material with each instance's
own colour multiplier on the crown. Run in the editor's Python:

    D:/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript ^
        -script="C:/dev/SAM3D-wt/terrain/tools/ue_build_tree_material.py" -unattended -nullrhi

The instances' custom data floats 0-2 hold the multiplier (simcore/scenery/tree_tint.hpp: the tree's autumn colour and its
own brightness, from the day of the year). The multiplier is laid on green-dominant vertex colours only (the crown, a
wood's mass: G over R), so a trunk keeps its bark. Without this material the map actor draws the trees in the plain vertex
colour material, as before.
"""
import unreal

MATERIAL_DIR = '/Game/SAM3D/Map'
MATERIAL = 'M_SimMapTree'


def build_material():
    path = MATERIAL_DIR + '/' + MATERIAL
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = tools.create_asset(MATERIAL, MATERIAL_DIR, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('used_with_instanced_static_meshes', True)
    lib = unreal.MaterialEditingLibrary
    vc = lib.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -900, 0)
    tint = lib.create_material_expression(mat, unreal.MaterialExpressionPerInstanceCustomData3Vector, -900, 300)
    tint.set_editor_property('data_index', 0)
    tint.set_editor_property('const_default_value', unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    # Crownness: 1 where the vertex colour is green over red (a crown or a mass), 0 on bark.
    sub = lib.create_material_expression(mat, unreal.MaterialExpressionSubtract, -700, 100)
    lib.connect_material_expressions(vc, 'G', sub, 'A')
    lib.connect_material_expressions(vc, 'R', sub, 'B')
    gain = lib.create_material_expression(mat, unreal.MaterialExpressionConstant, -700, 220)
    gain.set_editor_property('r', 40.0)
    mul = lib.create_material_expression(mat, unreal.MaterialExpressionMultiply, -520, 140)
    lib.connect_material_expressions(sub, '', mul, 'A')
    lib.connect_material_expressions(gain, '', mul, 'B')
    sat = lib.create_material_expression(mat, unreal.MaterialExpressionSaturate, -360, 140)
    lib.connect_material_expressions(mul, '', sat, '')
    one = lib.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -520, 420)
    one.set_editor_property('constant', unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    lerp = lib.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -200, 300)
    lib.connect_material_expressions(one, '', lerp, 'A')
    lib.connect_material_expressions(tint, '', lerp, 'B')
    lib.connect_material_expressions(sat, '', lerp, 'Alpha')
    colour = lib.create_material_expression(mat, unreal.MaterialExpressionMultiply, 0, 120)
    lib.connect_material_expressions(vc, '', colour, 'A')
    lib.connect_material_expressions(lerp, '', colour, 'B')
    rough = lib.create_material_expression(mat, unreal.MaterialExpressionConstant, 0, 300)
    rough.set_editor_property('r', 0.95)
    spec = lib.create_material_expression(mat, unreal.MaterialExpressionConstant, 0, 400)
    spec.set_editor_property('r', 0.2)
    lib.connect_material_property(colour, '', unreal.MaterialProperty.MP_BASE_COLOR)
    lib.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    lib.connect_material_property(spec, '', unreal.MaterialProperty.MP_SPECULAR)
    lib.recompile_material(mat)
    if lib.get_material_property_input_node(mat, unreal.MaterialProperty.MP_BASE_COLOR) is None:
        raise RuntimeError('the colour did not connect to the base colour')
    unreal.EditorAssetLibrary.save_asset(path)
    unreal.log('TREE MATERIAL: %s' % path)


build_material()
