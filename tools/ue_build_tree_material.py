"""Makes M_SimMapTree (roadmap T5.1, docs/scenery/T5_T8_PLAN.md): the map's vertex-colour material with each instance's
own colour multiplier on the crown. Run in the editor's Python:

    D:/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript ^
        -script="C:/dev/SAM3D-wt/terrain/tools/ue_build_tree_material.py" -unattended -nullrhi

The instances' custom data floats 0-2 hold the multiplier (simcore/scenery/tree_tint.hpp: the tree's autumn colour and its
own brightness, from the day of the year). The multiplier is laid on green-dominant vertex colours only (the crown, a
wood's mass: G over R), so a trunk keeps its bark. Without this material the map actor draws the trees in the plain vertex
colour material, as before.
"""
import json
import os

import unreal

ROOT = 'C:/dev/SAM3D-wt/terrain'
MATERIAL_DIR = '/Game/SAM3D/Map'
MATERIAL = 'M_SimMapTree'
CLUTTER = 'M_SimMapClutter'
FOLIAGE = 'M_SimFoliage'


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


def build_clutter_material():
    """M_SimMapClutter: the vertex colours times each instance's three custom-data floats (a plant's own brightness)."""
    path = MATERIAL_DIR + '/' + CLUTTER
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = tools.create_asset(CLUTTER, MATERIAL_DIR, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('used_with_instanced_static_meshes', True)
    lib = unreal.MaterialEditingLibrary
    vc = lib.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -500, 0)
    bright = lib.create_material_expression(mat, unreal.MaterialExpressionPerInstanceCustomData3Vector, -500, 200)
    bright.set_editor_property('data_index', 0)
    bright.set_editor_property('const_default_value', unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    colour = lib.create_material_expression(mat, unreal.MaterialExpressionMultiply, -250, 100)
    lib.connect_material_expressions(vc, '', colour, 'A')
    lib.connect_material_expressions(bright, '', colour, 'B')
    rough = lib.create_material_expression(mat, unreal.MaterialExpressionConstant, -250, 300)
    rough.set_editor_property('r', 0.95)
    lib.connect_material_property(colour, '', unreal.MaterialProperty.MP_BASE_COLOR)
    lib.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    lib.recompile_material(mat)
    if lib.get_material_property_input_node(mat, unreal.MaterialProperty.MP_BASE_COLOR) is None:
        raise RuntimeError('the colour did not connect to the base colour')
    unreal.EditorAssetLibrary.save_asset(path)
    unreal.log('CLUTTER MATERIAL: %s' % path)


def build_foliage_material():
    """M_SimFoliage: a registered plant model's own textures (BaseColorTexture with its alpha, NormalTexture) on a masked,
    two-sided, lit material, times the instance's colour multiplier (custom data 0-2, white where there is none): the import
    (tools/ue_import_model.py) makes one instance of it for each of the model's material slots."""
    path = MATERIAL_DIR + '/' + FOLIAGE
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = tools.create_asset(FOLIAGE, MATERIAL_DIR, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property('two_sided', True)
    mat.set_editor_property('opacity_mask_clip_value', 0.4)
    mat.set_editor_property('used_with_instanced_static_meshes', True)
    lib = unreal.MaterialEditingLibrary
    base = lib.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -700, 0)
    base.set_editor_property('parameter_name', 'BaseColorTexture')
    base.set_editor_property('texture', unreal.load_asset('/Engine/EngineResources/WhiteSquareTexture'))
    norm = lib.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -700, 300)
    norm.set_editor_property('parameter_name', 'NormalTexture')
    norm.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    norm.set_editor_property('texture', unreal.load_asset('/Engine/EngineMaterials/DefaultNormal'))
    tint = lib.create_material_expression(mat, unreal.MaterialExpressionPerInstanceCustomData3Vector, -700, 550)
    tint.set_editor_property('data_index', 0)
    tint.set_editor_property('const_default_value', unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    colour = lib.create_material_expression(mat, unreal.MaterialExpressionMultiply, -400, 100)
    lib.connect_material_expressions(base, 'RGB', colour, 'A')
    lib.connect_material_expressions(tint, '', colour, 'B')
    rough = lib.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 400)
    rough.set_editor_property('r', 0.85)
    lib.connect_material_property(colour, '', unreal.MaterialProperty.MP_BASE_COLOR)
    lib.connect_material_property(norm, 'RGB', unreal.MaterialProperty.MP_NORMAL)
    lib.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    colour_gain(mat, colour)
    near_hide(mat, base)
    lib.recompile_material(mat)
    if lib.get_material_property_input_node(mat, unreal.MaterialProperty.MP_OPACITY_MASK) is None:
        raise RuntimeError('the alpha did not connect to the opacity mask')
    unreal.EditorAssetLibrary.save_asset(path)
    unreal.log('FOLIAGE MATERIAL: %s' % path)


def near_hide(mat, base):
    """The opacity mask is the texture's alpha times 'not within NearHideM metres of the camera', measured from the
    instance's pivot (the local origin taken to the world: per instance on an instanced mesh), so a card tree near the
    camera is gone whole where its near model draws (A.3.26, SimMapActor). NearHideM 0, the default, hides nothing."""
    lib = unreal.MaterialEditingLibrary
    zero = lib.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -1100, 700)
    pivot = lib.create_material_expression(mat, unreal.MaterialExpressionTransformPosition, -900, 700)
    pivot.set_editor_property('transform_source_type', unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    pivot.set_editor_property('transform_type', unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
    lib.connect_material_expressions(zero, '', pivot, '')
    cam = lib.create_material_expression(mat, unreal.MaterialExpressionCameraPositionWS, -900, 820)
    dist = lib.create_material_expression(mat, unreal.MaterialExpressionDistance, -700, 750)
    lib.connect_material_expressions(pivot, '', dist, 'A')
    lib.connect_material_expressions(cam, '', dist, 'B')
    to_m = lib.create_material_expression(mat, unreal.MaterialExpressionMultiply, -550, 750)
    to_m.set_editor_property('const_b', 0.01)
    lib.connect_material_expressions(dist, '', to_m, 'A')
    param = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -550, 880)
    param.set_editor_property('parameter_name', 'NearHideM')
    param.set_editor_property('default_value', 0.0)
    beyond = lib.create_material_expression(mat, unreal.MaterialExpressionSubtract, -400, 800)
    lib.connect_material_expressions(to_m, '', beyond, 'A')
    lib.connect_material_expressions(param, '', beyond, 'B')
    sharp = lib.create_material_expression(mat, unreal.MaterialExpressionMultiply, -280, 800)
    sharp.set_editor_property('const_b', 1000.0)
    lib.connect_material_expressions(beyond, '', sharp, 'A')
    keep = lib.create_material_expression(mat, unreal.MaterialExpressionSaturate, -160, 800)
    lib.connect_material_expressions(sharp, '', keep, '')
    mask = lib.create_material_expression(mat, unreal.MaterialExpressionMultiply, -40, 650)
    lib.connect_material_expressions(base, 'A', mask, 'A')
    lib.connect_material_expressions(keep, '', mask, 'B')
    lib.connect_material_property(mask, '', unreal.MaterialProperty.MP_OPACITY_MASK)


def colour_gain(mat, colour):
    """The base colour times ColourGain (default 1, A.3.31): a slot whose texture is too light for the map (the oak's bark)
    is darkened per material instance, from its pick's bark_gain."""
    lib = unreal.MaterialEditingLibrary
    gain = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -400, 250)
    gain.set_editor_property('parameter_name', 'ColourGain')
    gain.set_editor_property('default_value', 1.0)
    out = lib.create_material_expression(mat, unreal.MaterialExpressionMultiply, -250, 150)
    lib.connect_material_expressions(colour, '', out, 'A')
    lib.connect_material_expressions(gain, '', out, 'B')
    lib.connect_material_property(out, '', unreal.MaterialProperty.MP_BASE_COLOR)


def patch_foliage_gains():
    """Adds ColourGain to the existing M_SimFoliage in place (if it has none) and sets each pick's bark_gain on its bark
    slots' instances (/Game/ThirdParty/Models/<id>/SimMaterials/*bark*)."""
    path = MATERIAL_DIR + '/' + FOLIAGE
    mat = unreal.load_asset(path)
    lib = unreal.MaterialEditingLibrary
    if 'ColourGain' not in [str(n) for n in lib.get_scalar_parameter_names(mat)]:
        colour = lib.get_material_property_input_node(mat, unreal.MaterialProperty.MP_BASE_COLOR)
        if not isinstance(colour, unreal.MaterialExpressionMultiply):
            raise RuntimeError('the base colour is not the texture times the tint: %s' % colour)
        colour_gain(mat, colour)
        lib.recompile_material(mat)
        unreal.EditorAssetLibrary.save_asset(path)
        unreal.log('FOLIAGE MATERIAL: ColourGain added to %s' % path)
    with open(ROOT + '/data/scenery/model_picks.json', encoding='utf-8') as f:
        picks = json.load(f)['picks']
    for pick in picks:
        gain = pick.get('bark_gain')
        if gain is None:
            continue
        folder = '/Game/ThirdParty/Models/%s/SimMaterials' % pick['id']
        for asset in unreal.EditorAssetLibrary.list_assets(folder, recursive=False):
            if 'bark' not in asset.lower():
                continue
            mi = unreal.load_asset(asset)
            lib.set_material_instance_scalar_parameter_value(mi, 'ColourGain', gain)
            lib.update_material_instance(mi)
            unreal.EditorAssetLibrary.save_loaded_asset(mi)
            unreal.log('FOLIAGE MATERIAL: %s ColourGain %.2f' % (mi.get_name(), gain))


def patch_foliage_near_hide():
    """Adds near_hide to the existing M_SimFoliage in place, keeping every card's instance of it."""
    path = MATERIAL_DIR + '/' + FOLIAGE
    mat = unreal.load_asset(path)
    lib = unreal.MaterialEditingLibrary
    if 'NearHideM' in [str(n) for n in lib.get_scalar_parameter_names(mat)]:
        unreal.log('FOLIAGE MATERIAL: NearHideM already there')
        return
    base = lib.get_material_property_input_node(mat, unreal.MaterialProperty.MP_OPACITY_MASK)
    if not isinstance(base, unreal.MaterialExpressionTextureSampleParameter2D):
        raise RuntimeError('the opacity mask is not the base texture\'s alpha: %s' % base)
    near_hide(mat, base)
    lib.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(path)
    unreal.log('FOLIAGE MATERIAL: NearHideM added to %s' % path)


if os.environ.get('SAM3D_FOLIAGE_GAINS') == '1':
    patch_foliage_gains()
elif os.environ.get('SAM3D_FOLIAGE_NEAR_ONLY') == '1':
    patch_foliage_near_hide()
else:
    build_material()
    build_clutter_material()
    build_foliage_material()
