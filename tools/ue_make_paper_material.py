# Make M_CabinPaper, the material a paper in a cabin is drawn on: the radio data on the PU-12M radiotelephonist's table
# and the talk tables on the commander's desk (docs/crew/12_COMMUNICATIONS.md). Paper is lit by the cabin's lamps like
# everything round it and gives off no light of its own, so unlike the tubes' M_CabinCRT this is an opaque, default-lit,
# matt material.
#
#     D:/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe C:/dev/SAM3D/unreal/SAM3D/SAM3D.uproject ^
#         -run=pythonscript -script="C:/dev/SAM3D/tools/ue_make_paper_material.py" -unattended -nullrhi
#
# (or in the open editor with Play stopped, through tools/ue_remote.py.)
#
# The cabin draws the typed sheet into a render target (SimCabinDisplayComponent::DrawDocument):
#   SlateUI              the render target: the paper's own colour and the ink
#   TintColorAndOpacity  a multiplier on it (1: the cabin component sets it)
#   Roughness            0.9: uncoated paper
import unreal

lib = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
MAT = '/Game/SAM3D/Cabin/Materials'
NAME = 'M_CabinPaper'


def expr(m, cls, x, y, **props):
    e = mel.create_material_expression(m, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def link(a, aout, b, bin_):
    if not mel.connect_material_expressions(a, aout, b, bin_):
        print('LINK FAILED', a.get_name(), repr(aout), '->', b.get_name(), repr(bin_))


path = MAT + '/' + NAME
if lib.does_asset_exist(path):
    m = unreal.load_asset(path)
    mel.delete_all_material_expressions(m)
else:
    m = tools.create_asset(NAME, MAT, unreal.Material, unreal.MaterialFactoryNew())
m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
m.set_editor_property('two_sided', False)

S, V = unreal.MaterialExpressionScalarParameter, unreal.MaterialExpressionVectorParameter

uv = expr(m, unreal.MaterialExpressionTextureCoordinate, -900, -100)
tex = expr(m, unreal.MaterialExpressionTextureSampleParameter2D, -650, -100, parameter_name='SlateUI',
           texture=unreal.load_asset('/Engine/EngineResources/DefaultTexture'))
link(uv, '', tex, 'UVs')
tint = expr(m, V, -650, 150, parameter_name='TintColorAndOpacity', default_value=unreal.LinearColor(1, 1, 1, 1))
col = expr(m, unreal.MaterialExpressionMultiply, -350, -50)
link(tex, 'RGB', col, 'A')
link(tint, '', col, 'B')
rough = expr(m, S, -350, 150, parameter_name='Roughness', default_value=0.9)
spec = expr(m, unreal.MaterialExpressionConstant, -350, 250, r=0.25)

mel.connect_material_property(col, '', unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
mel.connect_material_property(spec, '', unreal.MaterialProperty.MP_SPECULAR)
mel.recompile_material(m)
lib.save_asset(path)
print('made ' + path)
