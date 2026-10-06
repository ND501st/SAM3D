# Make M_CabinCRT, the material the cabin's cathode-ray displays are drawn through.
#
# Run it in the open editor with Play stopped, through tools/ue_remote.py (its file option).
#
# The cabin draws each display's picture (tube, TV, dial) into a render target with the Canvas; this unlit material
# shows that texture the way a 1970s-80s tube does, instead of as a flat, razor-sharp UI quad:
#   Curvature        barrel distortion of the picture (the glass bulges), 0.03-0.07
#   GlowAmount/Radius  a four-tap halo: phosphor bleeds into the dark round bright lines
#   ScanStrength/ScanLines  horizontal raster lines (the TV; the radial tubes use none)
#   NoiseStrength/Scale   shimmering noise over the picture, re-rolled every frame (receiver noise on the TV)
#   FlickerStrength  a slight mains-rate brightness flicker
#   VignetteStrength the picture fades towards the rim of the tube
#   ReflectStrength  a soft reflection of the cabin lamps in the convex glass, added on top (not tinted)
#   TintColorAndOpacity  RGB gain (the cabin camera's fixed exposure dims unlit surfaces; see SimCabinDisplayComponent)
#   SlateUI          the render target, as in the engine's Widget3DPassThrough material it replaces
# Parameters the old material had (BackColor, OpacityFromTexture) are not used; the code still sets them harmlessly.
import unreal

lib = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
MAT = '/Game/SAM3D/Cabin/Materials'
NAME = 'M_CabinCRT'


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
m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property('two_sided', True)

S, V = unreal.MaterialExpressionScalarParameter, unreal.MaterialExpressionVectorParameter
M, A, Sub = unreal.MaterialExpressionMultiply, unreal.MaterialExpressionAdd, unreal.MaterialExpressionSubtract


def scalar(name, default, y):
    return expr(m, S, -2600, y, parameter_name=name, default_value=default)


curv = scalar('Curvature', 0.04, -600)
glow_r = scalar('GlowRadius', 0.006, -450)
glow_a = scalar('GlowAmount', 0.35, -300)
scan_s = scalar('ScanStrength', 0.0, -150)
scan_n = scalar('ScanLines', 240.0, 0)
noise_s = scalar('NoiseStrength', 0.0, 150)
noise_n = scalar('NoiseScale', 160.0, 300)
flick = scalar('FlickerStrength', 0.02, 450)
vign = scalar('VignetteStrength', 0.3, 600)
refl = scalar('ReflectStrength', 0.5, 750)
tint = expr(m, V, -2600, 900, parameter_name='TintColorAndOpacity', default_value=unreal.LinearColor(4, 4, 4, 1))

uv = expr(m, unreal.MaterialExpressionTextureCoordinate, -2300, 0)
# centred coordinates c = uv * 2 - 1, and the barrel distortion uvd = (c * (1 + k |c|^2)) * 0.5 + 0.5
c2 = expr(m, M, -2100, 0, const_b=2.0); link(uv, '', c2, 'A')
c = expr(m, Sub, -1950, 0, const_b=1.0); link(c2, '', c, 'A')
r2 = expr(m, unreal.MaterialExpressionDotProduct, -1800, 100); link(c, '', r2, 'A'); link(c, '', r2, 'B')
kr = expr(m, M, -1650, 100); link(r2, '', kr, 'A'); link(curv, '', kr, 'B')
f = expr(m, A, -1500, 100, const_b=1.0); link(kr, '', f, 'A')
cd = expr(m, M, -1350, 0); link(c, '', cd, 'A'); link(f, '', cd, 'B')
u1 = expr(m, M, -1200, 0, const_b=0.5); link(cd, '', u1, 'A')
uvd = expr(m, A, -1050, 0, const_b=0.5); link(u1, '', uvd, 'A')

tex = unreal.load_asset('/Engine/EngineResources/DefaultTexture')


def sample(uvs, x, y):
    t = expr(m, unreal.MaterialExpressionTextureSampleParameter2D, x, y, parameter_name='SlateUI', texture=tex)
    link(uvs, '', t, 'UVs')
    return t


main = sample(uvd, -850, 0)
# four-tap halo
taps = []
for i, (dx, dy) in enumerate(((1, 0), (-1, 0), (0, 1), (0, -1))):
    off = expr(m, unreal.MaterialExpressionConstant2Vector, -1100, 250 + 120 * i, r=float(dx), g=float(dy))
    mo = expr(m, M, -950, 250 + 120 * i); link(off, '', mo, 'A'); link(glow_r, '', mo, 'B')
    ao = expr(m, A, -800, 250 + 120 * i); link(uvd, '', ao, 'A'); link(mo, '', ao, 'B')
    taps.append(sample(ao, -650, 250 + 120 * i))
s01 = expr(m, A, -450, 300); link(taps[0], 'RGB', s01, 'A'); link(taps[1], 'RGB', s01, 'B')
s23 = expr(m, A, -450, 420); link(taps[2], 'RGB', s23, 'A'); link(taps[3], 'RGB', s23, 'B')
ssum = expr(m, A, -300, 360); link(s01, '', ssum, 'A'); link(s23, '', ssum, 'B')
blur = expr(m, M, -150, 360, const_b=0.25); link(ssum, '', blur, 'A')
glow = expr(m, M, 0, 360); link(blur, '', glow, 'A'); link(glow_a, '', glow, 'B')
base = expr(m, A, 150, 100); link(main, 'RGB', base, 'A'); link(glow, '', base, 'B')

# raster lines: 1 - ScanStrength * (0.5 + 0.5 sin(2 pi * v * ScanLines))
uy = expr(m, unreal.MaterialExpressionComponentMask, -1800, 800, r=False, g=True, b=False, a=False); link(uv, '', uy, '')
ln = expr(m, M, -1650, 800); link(uy, '', ln, 'A'); link(scan_n, '', ln, 'B')
sn = expr(m, unreal.MaterialExpressionSine, -1500, 800); link(ln, '', sn, '')
sh = expr(m, M, -1350, 800, const_b=0.5); link(sn, '', sh, 'A')
sh2 = expr(m, A, -1200, 800, const_b=0.5); link(sh, '', sh2, 'A')
ss = expr(m, M, -1050, 800); link(sh2, '', ss, 'A'); link(scan_s, '', ss, 'B')
scan = expr(m, unreal.MaterialExpressionOneMinus, -900, 800); link(ss, '', scan, '')

# noise, new every frame: 1 + NoiseStrength * noise(uv * NoiseScale, 30 t)
t = expr(m, unreal.MaterialExpressionTime, -1800, 1000)
t30 = expr(m, M, -1650, 1000, const_b=30.0); link(t, '', t30, 'A')
un = expr(m, M, -1650, 1100); link(uv, '', un, 'A'); link(noise_n, '', un, 'B')
pos = expr(m, unreal.MaterialExpressionAppendVector, -1500, 1050); link(un, '', pos, 'A'); link(t30, '', pos, 'B')
nz = expr(m, unreal.MaterialExpressionNoise, -1350, 1050, scale=1.0, quality=1, output_min=-1.0, output_max=1.0)
link(pos, '', nz, 'World Position')
nm = expr(m, M, -1200, 1050); link(nz, '', nm, 'A'); link(noise_s, '', nm, 'B')
nf = expr(m, A, -1050, 1050, const_b=1.0); link(nm, '', nf, 'A')

# flicker: 1 + FlickerStrength * sin(2 pi * 7.3 t)
t7 = expr(m, M, -1650, 1250, const_b=7.3); link(t, '', t7, 'A')
fs = expr(m, unreal.MaterialExpressionSine, -1500, 1250); link(t7, '', fs, '')
fm = expr(m, M, -1350, 1250); link(fs, '', fm, 'A'); link(flick, '', fm, 'B')
ff = expr(m, A, -1200, 1250, const_b=1.0); link(fm, '', ff, 'A')

# vignette: saturate(1 - VignetteStrength |c|^2)
vm = expr(m, M, -1500, 1450); link(r2, '', vm, 'A'); link(vign, '', vm, 'B')
vo = expr(m, unreal.MaterialExpressionOneMinus, -1350, 1450); link(vm, '', vo, '')
vg = expr(m, unreal.MaterialExpressionSaturate, -1200, 1450); link(vo, '', vg, '')

m1 = expr(m, M, 350, 100); link(base, '', m1, 'A'); link(scan, '', m1, 'B')
m2 = expr(m, M, 500, 100); link(m1, '', m2, 'A'); link(nf, '', m2, 'B')
m3 = expr(m, M, 650, 100); link(m2, '', m3, 'A'); link(ff, '', m3, 'B')
m4 = expr(m, M, 800, 100); link(m3, '', m4, 'A'); link(vg, '', m4, 'B')
m5 = expr(m, M, 950, 100); link(m4, '', m5, 'A'); link(tint, '', m5, 'B')

# a soft reflection of the lamps in the glass: a lobe centred up and to the left, added after the tint
pt = expr(m, unreal.MaterialExpressionConstant2Vector, -1500, 1650, r=-0.55, g=-0.6)
dist = expr(m, unreal.MaterialExpressionDistance, -1350, 1650); link(c, '', dist, 'A'); link(pt, '', dist, 'B')
d1 = expr(m, M, -1200, 1650, const_b=0.9); link(dist, '', d1, 'A')
d2 = expr(m, unreal.MaterialExpressionOneMinus, -1050, 1650); link(d1, '', d2, '')
d3 = expr(m, unreal.MaterialExpressionSaturate, -900, 1650); link(d2, '', d3, '')
d4 = expr(m, unreal.MaterialExpressionPower, -750, 1650, const_exponent=2.0); link(d3, '', d4, 'Base')
rm = expr(m, M, -600, 1650); link(d4, '', rm, 'A'); link(refl, '', rm, 'B')
rc = expr(m, unreal.MaterialExpressionConstant3Vector, -600, 1800, constant=unreal.LinearColor(0.8, 0.85, 0.9, 1))
rcol = expr(m, M, -450, 1700); link(rm, '', rcol, 'A'); link(rc, '', rcol, 'B')
out = expr(m, A, 1100, 100); link(m5, '', out, 'A'); link(rcol, '', out, 'B')

mel.connect_material_property(out, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(m)
lib.save_asset(m.get_path_name())
print('made', m.get_path_name())
