# Make M_CabinGlassBoard, the material the PU-12M's plotting board is drawn through: a sheet of clear plexiglass with
# frosted etching and grease pencil on it (docs/research/PLOTTING_BOARD_2026-10.md).
#
#     D:/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe C:/dev/SAM3D/unreal/SAM3D/SAM3D.uproject ^
#         -run=pythonscript -script="C:/dev/SAM3D/tools/ue_make_glass_material.py" -unattended -nullrhi
#
# (or in the open editor with Play stopped, through tools/ue_remote.py.)
#
# The cabin draws the board into a render target on black (SimCabinDisplayComponent::DrawPlotBoard); this unlit,
# translucent material shows black as clear glass and whatever is drawn as itself, the opacity rising with its
# brightness:
#   SlateUI              the render target
#   TintColorAndOpacity  RGB gain of the drawing (the cabin camera's fixed exposure)
#   GlassOpacity         how much of the clear sheet shows (a faint grey haze), 0.04
#   InkOpacity           how fast a drawn line turns opaque with its brightness, 3
#   GlassTint            the sheet's own faint colour
#   ReflectStrength      a soft reflection of the cabin lamps across the sheet
import unreal

lib = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
MAT = '/Game/SAM3D/Cabin/Materials'
NAME = 'M_CabinGlassBoard'


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
m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property('two_sided', True)

S, V = unreal.MaterialExpressionScalarParameter, unreal.MaterialExpressionVectorParameter
M, A = unreal.MaterialExpressionMultiply, unreal.MaterialExpressionAdd

glass_op = expr(m, S, -1600, 400, parameter_name='GlassOpacity', default_value=0.04)
ink_op = expr(m, S, -1600, 500, parameter_name='InkOpacity', default_value=3.0)
refl = expr(m, S, -1600, 700, parameter_name='ReflectStrength', default_value=0.25)
tint = expr(m, V, -1600, 0, parameter_name='TintColorAndOpacity', default_value=unreal.LinearColor(2.5, 2.5, 2.5, 1))
glass_tint = expr(m, V, -1600, 200, parameter_name='GlassTint', default_value=unreal.LinearColor(0.10, 0.12, 0.12, 1))

uv = expr(m, unreal.MaterialExpressionTextureCoordinate, -1600, -200)
tex = expr(m, unreal.MaterialExpressionTextureSampleParameter2D, -1350, -200, parameter_name='SlateUI',
           texture=unreal.load_asset('/Engine/EngineResources/DefaultTexture'))
link(uv, '', tex, 'UVs')

# the drawing's brightness: the largest of its channels
mx1 = expr(m, unreal.MaterialExpressionMax, -1100, 300)
link(tex, 'R', mx1, 'A'); link(tex, 'G', mx1, 'B')
mx = expr(m, unreal.MaterialExpressionMax, -950, 300)
link(mx1, '', mx, 'A'); link(tex, 'B', mx, 'B')
# opacity = saturate(GlassOpacity + InkOpacity * brightness)
io = expr(m, M, -800, 400); link(mx, '', io, 'A'); link(ink_op, '', io, 'B')
op = expr(m, A, -650, 400); link(io, '', op, 'A'); link(glass_op, '', op, 'B')
ops = expr(m, unreal.MaterialExpressionSaturate, -500, 400); link(op, '', ops, '')

# colour: the drawing under its gain, on the glass's faint tint
dr = expr(m, M, -1100, 0); link(tex, 'RGB', dr, 'A'); link(tint, '', dr, 'B')
col = expr(m, A, -800, 0); link(dr, '', col, 'A'); link(glass_tint, '', col, 'B')

# a soft lamp reflection across the sheet, from the upper left
c2 = expr(m, M, -1350, 800, const_b=2.0); link(uv, '', c2, 'A')
c = expr(m, unreal.MaterialExpressionSubtract, -1200, 800, const_b=1.0); link(c2, '', c, 'A')
pt = expr(m, unreal.MaterialExpressionConstant2Vector, -1200, 950, r=-0.5, g=-0.55)
dist = expr(m, unreal.MaterialExpressionDistance, -1050, 850); link(c, '', dist, 'A'); link(pt, '', dist, 'B')
d1 = expr(m, M, -900, 850, const_b=0.8); link(dist, '', d1, 'A')
d2 = expr(m, unreal.MaterialExpressionOneMinus, -750, 850); link(d1, '', d2, '')
d3 = expr(m, unreal.MaterialExpressionSaturate, -600, 850); link(d2, '', d3, '')
d4 = expr(m, unreal.MaterialExpressionPower, -450, 850, const_exponent=2.0); link(d3, '', d4, 'Base')
rm = expr(m, M, -300, 850); link(d4, '', rm, 'A'); link(refl, '', rm, 'B')
out = expr(m, A, -300, 0); link(col, '', out, 'A'); link(rm, '', out, 'B')
opr = expr(m, A, -300, 450); link(ops, '', opr, 'A'); link(rm, '', opr, 'B')
opf = expr(m, unreal.MaterialExpressionSaturate, -150, 450); link(opr, '', opf, '')

mel.connect_material_property(out, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.connect_material_property(opf, '', unreal.MaterialProperty.MP_OPACITY)
mel.recompile_material(m)
lib.save_asset(m.get_path_name())
print('made', m.get_path_name())
