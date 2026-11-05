"""Makes M_SimMapCrop (roadmap L5, docs/scenery/TERRAIN_ROADMAP.md A.3.6-A.3.7): the standing crops' blocks
(simcore/scenery/standing_crops.hpp, ASimMapActor::BuildCrops) drawn as plants, and dithered away near the camera so the
crop's plants (the clutter ring's maize cards, data/scenery/clutter.json) stand in their place. Run in the editor's Python:

    D:/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript ^
        -script="C:/dev/SAM3D-wt/terrain/tools/ue_build_crop_material.py" -unattended -nullrhi

The block's UVs (StandingCrops::BlockMesh with uvs): on a wall, metres along it and the height as a share of the stand; on
the top, metres along the field's rows and across them. The walls are drawn as plants, each 23 cm slot of a face a plant
with its own tone, a stalk and a dark gap where the rows go in, darker at the foot; their top is cut ragged (each plant's
own height, 0.86-1.06 of the stand) over the top, which lies under the fringe; the top is drawn in the rows, 75 cm apart.
Masked: the opacity is that ragged cut times the dither of saturate((distance to the camera - FadeFromCm) / (FadeToCm -
FadeFromCm)), so the block is gone within FadeFromCm and whole beyond FadeToCm. The map actor sets both from clutter.json's
crop_fade_m on a dynamic instance. Without this material the blocks are drawn whole in the plain vertex colour material.
"""
import unreal

MATERIAL_DIR = '/Game/SAM3D/Map'
MATERIAL = 'M_SimMapCrop'

TONE = r'''
float wall = 1.0 - saturate(abs(N.z) * 2.0 - 0.5);
float slot = floor(UV.x / 0.23);
float r1 = frac(sin(slot * 12.9898) * 43758.5453);
float r2 = frac(sin(slot * 78.233) * 43758.5453);
float x = frac(UV.x / 0.23) - 0.5;
float h = UV.y;
float stalk = 1.0 - smoothstep(0.05, 0.10, abs(x - (r2 - 0.5) * 0.4));
float leafy = 0.5 + 0.5 * sin(h * 23.0 + r1 * 6.2832 + UV.x * 3.1);
float low = smoothstep(-0.05, 0.5, h);
float wallTone = lerp(0.45, 1.0, low) * (0.75 + 0.3 * r1) * (0.82 + 0.18 * leafy) * (1.0 + 0.2 * stalk);
float gap = smoothstep(0.28, 0.48, abs(x)) * (0.3 + 0.45 * (1.0 - low));
wallTone *= 1.0 - 0.65 * gap;
// Afar a pixel spans many plants: their average, not a shimmer.
float wallDetail = saturate(1.0 - fwidth(UV.x) / 0.15);
wallTone = lerp(lerp(0.5, 0.95, low) * 0.85, wallTone, wallDetail);
// The top: the rows faint, fading where a pixel spans a row; broad lumps of plants leaning and gaps.
float rowDetail = saturate(1.0 - max(fwidth(UV.x), fwidth(UV.y)) / 0.35);
float row = 0.5 + 0.5 * cos(UV.y * 6.2832 / 0.75);
// Value noise, two octaves (some 3 m and 1 m): plants leaning together and gaps, no pattern.
float lump = 0.0;
float2 p = UV * 0.33;
for (int o = 0; o < 2; o++) {
    float2 i = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = frac(sin(dot(i, float2(127.1, 311.7))) * 43758.5453);
    float b = frac(sin(dot(i + float2(1.0, 0.0), float2(127.1, 311.7))) * 43758.5453);
    float c = frac(sin(dot(i + float2(0.0, 1.0), float2(127.1, 311.7))) * 43758.5453);
    float d = frac(sin(dot(i + float2(1.0, 1.0), float2(127.1, 311.7))) * 43758.5453);
    lump += lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y) * (o == 0 ? 0.65 : 0.35);
    p = p * 2.9 + 17.0;
}
float topTone = (1.0 - 0.18 * rowDetail * (1.0 - row)) * (0.82 + 0.28 * lump);
return C * lerp(topTone, wallTone, wall);
'''

RAGGED = r'''
if (abs(N.z) > 0.5) return Fade;
float slot = floor(UV.x / 0.23);
float r1 = frac(sin(slot * 12.9898) * 43758.5453);
float r3 = frac(sin(slot * 39.425) * 43758.5453);
float x = frac(UV.x / 0.23) - 0.5;
float edge = 0.86 + 0.20 * r1 - 0.10 * abs(x) * (1.0 + r3) + 0.025 * sin(UV.x * 41.0 + r3 * 6.2832);
return (UV.y < edge ? 1.0 : 0.0) * Fade;
'''


def custom(lib, mat, code, inputs, out_type, x, y):
    node = lib.create_material_expression(mat, unreal.MaterialExpressionCustom, x, y)
    node.set_editor_property('code', code)
    node.set_editor_property('output_type', out_type)
    pins = []
    for name in inputs:
        pin = unreal.CustomInput()
        pin.set_editor_property('input_name', name)
        pins.append(pin)
    node.set_editor_property('inputs', pins)
    return node


def build():
    path = MATERIAL_DIR + '/' + MATERIAL
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = tools.create_asset(MATERIAL, MATERIAL_DIR, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property('opacity_mask_clip_value', 0.5)
    lib = unreal.MaterialEditingLibrary
    vc = lib.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -900, -300)
    uv = lib.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -900, -150)
    nrm = lib.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -900, -50)
    cam = lib.create_material_expression(mat, unreal.MaterialExpressionCameraPositionWS, -1200, 100)
    world = lib.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1200, 250)
    dist = lib.create_material_expression(mat, unreal.MaterialExpressionDistance, -1000, 150)
    lib.connect_material_expressions(cam, '', dist, 'A')
    lib.connect_material_expressions(world, '', dist, 'B')
    fade_from = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -1000, 350)
    fade_from.set_editor_property('parameter_name', 'FadeFromCm')
    fade_from.set_editor_property('default_value', 2800.0)
    fade_to = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -1000, 500)
    fade_to.set_editor_property('parameter_name', 'FadeToCm')
    fade_to.set_editor_property('default_value', 4200.0)
    over = lib.create_material_expression(mat, unreal.MaterialExpressionSubtract, -800, 200)
    lib.connect_material_expressions(dist, '', over, 'A')
    lib.connect_material_expressions(fade_from, '', over, 'B')
    span = lib.create_material_expression(mat, unreal.MaterialExpressionSubtract, -800, 450)
    lib.connect_material_expressions(fade_to, '', span, 'A')
    lib.connect_material_expressions(fade_from, '', span, 'B')
    share = lib.create_material_expression(mat, unreal.MaterialExpressionDivide, -600, 300)
    lib.connect_material_expressions(over, '', share, 'A')
    lib.connect_material_expressions(span, '', share, 'B')
    sat = lib.create_material_expression(mat, unreal.MaterialExpressionSaturate, -450, 300)
    lib.connect_material_expressions(share, '', sat, '')
    dither = lib.create_material_expression(mat, unreal.MaterialExpressionMaterialFunctionCall, -300, 300)
    func = unreal.load_asset('/Engine/Functions/Engine_MaterialFunctions02/Utility/DitherTemporalAA.DitherTemporalAA')
    if func is None:
        raise RuntimeError('no DitherTemporalAA material function')
    dither.set_editor_property('material_function', func)
    if not lib.connect_material_expressions(sat, '', dither, 'Alpha Threshold'):
        raise RuntimeError('the fade did not connect to DitherTemporalAA')
    tone = custom(lib, mat, TONE, ['UV', 'N', 'C'], unreal.CustomMaterialOutputType.CMOT_FLOAT3, -300, -200)
    for src, pin in ((uv, 'UV'), (nrm, 'N'), (vc, 'C')):
        if not lib.connect_material_expressions(src, '', tone, pin):
            raise RuntimeError('the tone did not take ' + pin)
    ragged = custom(lib, mat, RAGGED, ['UV', 'N', 'Fade'], unreal.CustomMaterialOutputType.CMOT_FLOAT1, -100, 300)
    for src, pin in ((uv, 'UV'), (nrm, 'N'), (dither, 'Fade')):
        if not lib.connect_material_expressions(src, '', ragged, pin):
            raise RuntimeError('the ragged top did not take ' + pin)
    rough = lib.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 0)
    rough.set_editor_property('r', 0.95)
    lib.connect_material_property(tone, '', unreal.MaterialProperty.MP_BASE_COLOR)
    lib.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    lib.connect_material_property(ragged, '', unreal.MaterialProperty.MP_OPACITY_MASK)
    lib.recompile_material(mat)
    if lib.get_material_property_input_node(mat, unreal.MaterialProperty.MP_OPACITY_MASK) is None:
        raise RuntimeError('the fade did not connect to the opacity mask')
    unreal.EditorAssetLibrary.save_asset(path)
    unreal.log('CROP MATERIAL: %s' % path)


build()
