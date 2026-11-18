"""Import the weapons effects' art and build their materials (docs/fx/README.md section 4.1).

Imports art/fx/textures/*.png (tools/fx_textures.py) into /Game/SAM3D/FX/Textures and art/fx/sounds/*.wav
(tools/fx_sounds.py) into /Game/SAM3D/FX/Sounds, replacing what is there, sets the textures' sampling and the loops'
looping, and builds the materials in /Game/SAM3D/FX/Materials (rebuilt from nothing when they exist). USimFxSubsystem
loads them by these names and falls back to the engine's basic shape material when one is missing.

Headless, with the editor closed (from PowerShell):
    & 'D:\\UE_5.8\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe' C:\\dev\\SAM3D\\unreal\\SAM3D\\SAM3D.uproject `
        -run=pythonscript -script=C:\\dev\\SAM3D\\tools\\ue_make_fx_assets.py -unattended -nullrhi
It prints one line per asset made and "FX ASSETS OK" at the end, or "FX ASSETS FAILED: <reason>". Printed lines go to
LogPython at Log verbosity: they are in unreal/SAM3D/Saved/Logs/SAM3D.log, and on the console only with
-FullStdOutLogOutput; failures are errors, shown either way.

The materials' contract (the C++ that draws with them is written to the same text):
  Sprites are /Engine/BasicShapes/Plane instances (100 x 100 cm, normal +Z, UV 0..1) in instanced static meshes with 13
  per-instance custom data floats: 0 frame (atlas cell / flipbook frame, row-major from the top-left), 1 opacity,
  2 heat, 3-5 tint RGB, 6 erosion, 7 lit, 8 kind (M_FX_Smoke's picture: 0 atlas, 1 generated fireball flipbook,
  2 colour flipbook; M_FX_Additive's: 3 the spiky flash, 4 the afterburner's flame), 9-11 the flashes' and fires' light on it RGB (T9, in SunColor's units), 12 fade_cm (the
  DepthFade distance). Every sprite material is unlit, two-sided, used with instanced static meshes, writes no depth
  and is fogged per pixel. Every texture is never streamed and sampled trilinear. The cell's UV: cell = floor(frame),
  col = fmod(cell, Cols), row = floor(cell / Cols), uv = (texcoord + float2(col, row)) / float2(Cols, Rows).
  M_FX_Smoke     premultiplied (AlphaComposite): smoke, debris and fire in one draw, sorted back to front together.
                 light = AmbientColor sky + SunColor share + local (floats 9-11); the sun's share by pixel on a sphere:
                   p = (WorldPosition - ObjectPosition) / (fade_cm / 0.6), its length at most 1; N = normalize(p + 0.6
                   CameraVector); share = 0.75 saturate(0.3 + 0.7 N.SunDir) + 0.25 lit. SunDir: towards the sun;
                   sky = lerp(SkyLow, 1, saturate(0.5 + 0.5 N.z)).
                 0 Atlas (T_FX_SmokeAtlas) 4 x 4: R lit from above, G thinness, B erosion field, A density.
                   a = A smoothstep(erosion, erosion + 0.15, B); E = (tint (AmbientColor sky + SunColor share R + local)
                   Brightness + FireColor heat 4 A) a
                 1 Flipbook (T_FX_Fireball) 8 x 8: R temperature, G lit, B billow, A density. temp = saturate(R heat);
                   a as 0; E = (tint (AmbientColor sky + SunColor share G + local) Brightness
                   + HeatRamp(temp, 0.5).rgb EmissiveScale temp^4) a
                 2 ColourFlipbook (T_FXR_Unity_Explosion02HD_5x5) 5 x 5, straight alpha, and EmissionLog (its _EmissionLog):
                   L = 161^v - 1, w = saturate((L - 0.6) / 1.4); a = C.a smoothstep(erosion, erosion + 0.2, C.a);
                   E = (lerp(C, tint, w) light Brightness + C L w saturate(heat)^4 EmissiveScale ColourGlow) a
                 Then E and a times opacity, DepthFade(fade_cm) and saturate((PixelDepth - 100) / 300); the C++ also
                 fades a whole sprite near the camera by its size (float 1, SimFxLogic::CameraFade).
  M_FX_Additive  additive     Tex (T_FX_Glow; spark, streak, ring, plume by parameter) Cols x Rows 1 x 1, Flash
                 (T_FX_Flash, the detonation's spiky flash) where kind is 3 and Afterburner (T_FX_Afterburner, a jet's
                 flame) where kind is 4: v = lerp(lerp(Tex.r, Flash.r, saturate(kind - 2)), Afterburner.r, saturate(kind - 3)).
                 Emissive = v tint opacity Intensity (1 + 4 heat)
  M_FX_Ribbon    translucent, not instanced (a procedural mesh). UV0 (u by distance along the trail, v across it), UV1
                 (erosion, hairline share), UV2 and UV3.x the frame's lights on it RGB (T9, already times its albedo, in
                 the vertex colour's units before Brightness). Tex = T_FX_Contrail(u UScale, v): R lit from the billows,
                 G thinness, B erosion field, A density.
                 Emissive = VertexColor.rgb Brightness lerp(0.55, 1, Tex.R) + (UV2.x, UV2.y, UV3.x);
                 Opacity = VertexColor.a lerp(Tex.A smoothstep(e, e + 0.2, Tex.B), 0.8, UV1.y)
                   saturate((PixelDepth - 100 - 100 w) / (300 + 300 w)), e = UV1.x, w = UV3.y (the trail's true width, m)
  M_FX_Munition  opaque, lit: BaseColor = VertexColor.rgb, Roughness 0.45, Metallic 0.1, Specular 0.5
  M_FX_Scorch    deferred decal, translucent: BaseColor = Tex.rgb, Roughness 0.95, Opacity = Tex.a Opacity (the decal
                 writes colour and roughness: UE 5.8 infers that from the pins connected; DecalBlendMode is gone)
"""
import glob
import json
import os

try:
    import unreal
    if not hasattr(unreal, 'EditorAssetLibrary'):   # outside the editor: the repo's unreal/ folder, a namespace package
        unreal = None
except ImportError:   # imported by tools/test_fx_tools.py outside the editor: the tables only
    unreal = None

ROOT = '/Game/SAM3D/FX'
TEX = ROOT + '/Textures'
SND = ROOT + '/Sounds'
MAT = ROOT + '/Materials'

# Packed data (sRGB off: the channels are numbers, not colours) and colour textures (sRGB on).
DATA_TEXTURES = ('T_FX_SmokeAtlas', 'T_FX_Fireball', 'T_FX_Glow', 'T_FX_Flash', 'T_FX_Streak', 'T_FX_Spark', 'T_FX_Ring',
                 'T_FX_Contrail', 'T_FX_Plume', 'T_FX_Afterburner', 'T_FX_DebrisAtlas')
COLOUR_TEXTURES = ('T_FX_HeatRamp', 'T_FX_Scorch')
CLAMPED_NO_MIPS = ('T_FX_HeatRamp',)   # a lookup: no wrap from white-hot back to soot, no mips to blur it

# The fetched CC0 flipbooks the materials use (art/fx/recorded/PROVENANCE.md 1.1): the Unity Labs explosion, a colour
# picture with straight alpha, and its HDR brightness, log encoded (L = 161^v - 1).
RECORDED_DIR = ('art', 'fx', 'recorded', 'textures', 'unity')
RECORDED_TEXTURES = ('T_FXR_Unity_Explosion02HD_5x5', 'T_FXR_Unity_Explosion02HD_5x5_EmissionLog')
RECORDED_DATA = ('T_FXR_Unity_Explosion02HD_5x5_EmissionLog',)

# Compression by texture (the art review's D1: TC_DEFAULT's BC1/BC3 kept the fireball's temperature in 5:6:5 block
# end points, about 32 levels, and the radial glows came out in steps): BC7 for the packed RGBA data and the colour
# flipbook, one 8-bit channel for the grey sprites, uncompressed for the heat ramp's 256 colours.
COMPRESSION = {
    'T_FX_SmokeAtlas': 'TC_BC7', 'T_FX_DebrisAtlas': 'TC_BC7', 'T_FX_Contrail': 'TC_BC7', 'T_FX_Fireball': 'TC_BC7',
    'T_FX_Glow': 'TC_GRAYSCALE', 'T_FX_Flash': 'TC_GRAYSCALE', 'T_FX_Spark': 'TC_GRAYSCALE', 'T_FX_Streak': 'TC_GRAYSCALE', 'T_FX_Ring': 'TC_GRAYSCALE',
    'T_FX_Plume': 'TC_GRAYSCALE', 'T_FX_Afterburner': 'TC_GRAYSCALE', 'T_FX_HeatRamp': 'TC_EDITOR_ICON', 'T_FX_Scorch': 'TC_DEFAULT',
    'T_FXR_Unity_Explosion02HD_5x5': 'TC_BC7', 'T_FXR_Unity_Explosion02HD_5x5_EmissionLog': 'TC_GRAYSCALE',
}

# Imported at most this size (the fx-final check: the 2048^2 colour flipbook and its emission map, about 14 MB never
# streamed; at 1024^2 a frame is 204 px, more than a fireball covers on the screen but at a few hundred metres).
MAX_SIZE = {'T_FXR_Unity_Explosion02HD_5x5': 1024, 'T_FXR_Unity_Explosion02HD_5x5_EmissionLog': 1024}

MATERIALS = ('M_FX_Smoke', 'M_FX_Additive', 'M_FX_Ribbon', 'M_FX_Munition', 'M_FX_Scorch')

# Per-instance custom data: index, name, the default when a material is previewed without instances.
CUSTOM_DATA = ((0, 'frame', 0.0), (1, 'opacity', 1.0), (2, 'heat', 0.0), (3, 'tint_r', 1.0), (4, 'tint_g', 1.0),
               (5, 'tint_b', 1.0), (6, 'erosion', 0.0), (7, 'lit', 1.0), (8, 'kind', 0.0), (9, 'local_r', 0.0),
               (10, 'local_g', 0.0), (11, 'local_b', 0.0), (12, 'fade_cm', 100.0))

# M_FX_Smoke's sprites by float 8: the smoke (or debris) atlas, the generated fireball flipbook, the colour flipbook.
KIND_ATLAS, KIND_HEAT_FLIPBOOK, KIND_COLOUR_FLIPBOOK = 0, 1, 2

# If art/fx/sounds/sounds.json is missing: the loops by their names (as tools/fx_sounds.py names them).
LOOP_NAME_PARTS = ('Jet_', 'Afterburner', 'Rocket_Motor', 'Prop_', 'Rotor_', 'Fire_Loop', '_Quad', '_Twin', '_Gatling')


def repo_root():
    if unreal is not None:
        # .../unreal/SAM3D/ -> the repository (works whether or not the commandlet sets __file__)
        return os.path.normpath(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()),
                                             '..', '..'))
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def loop_names(sounds_dir, names):
    """The sounds that loop: from fx_sounds.py's manifest (sounds.json, "loop": true), else by their names."""
    path = os.path.join(sounds_dir, 'sounds.json')
    if os.path.exists(path):
        with open(path, encoding='utf-8') as f:
            manifest = json.load(f)
        return {k for k, v in manifest.items() if v.get('loop')}, True
    return {n for n in names if any(p in n for p in LOOP_NAME_PARTS)}, False


# ---------------------------------------------------------------- the editor's side
FAILURES = []


def fail(why):
    FAILURES.append(why)
    unreal.log_error('FX ERROR: ' + why)   # an error: on the console and in the commandlet's summary


def enum_value(enum, names):
    """The first of these names the editor's Python gives the enum (its naming of prefixed values varies)."""
    for n in names:
        if hasattr(enum, n):
            return getattr(enum, n)
    fail('%s has none of %s (it has %s)' % (enum.__name__, names, [k for k in dir(enum) if k.isupper()]))
    return None


def made(path):
    print('made ' + path)


def setp(obj, name, value, required=True):
    """Set an editor property; a missing one is a failure if required, else a warning line."""
    try:
        obj.set_editor_property(name, value)
        return True
    except Exception as e:  # noqa: BLE001 - the editor raises its own types
        if required:
            fail('%s.%s: %s' % (obj.get_name(), name, e))
        else:
            print('FX WARNING: %s.%s not set: %s' % (obj.get_name(), name, e))
        return False


def import_files(files, dest):
    tasks = []
    for f in files:
        t = unreal.AssetImportTask()
        t.set_editor_property('filename', f)
        t.set_editor_property('destination_path', dest)
        t.set_editor_property('destination_name', os.path.splitext(os.path.basename(f))[0])
        t.set_editor_property('replace_existing', True)
        t.set_editor_property('automated', True)
        t.set_editor_property('save', True)
        tasks.append(t)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    return tasks


def import_textures(repo):
    lib = unreal.EditorAssetLibrary
    files = sorted(glob.glob(os.path.join(repo, 'art', 'fx', 'textures', '*.png')))
    known = set(DATA_TEXTURES) | set(COLOUR_TEXTURES)
    names = [os.path.splitext(os.path.basename(f))[0] for f in files]
    for n in sorted(known - set(names)):
        fail('no art/fx/textures/%s.png (run tools/fx_textures.py)' % n)
    for n in sorted(set(names) - known):
        print('FX WARNING: %s.png is not in the materials\' list: imported as a colour texture' % n)
    recorded = [os.path.join(repo, *RECORDED_DIR) + os.sep + n + '.png' for n in RECORDED_TEXTURES]
    for f in recorded:
        if os.path.exists(f):
            files.append(f)
            names.append(os.path.splitext(os.path.basename(f))[0])
        else:   # the material then keeps the generated fireball (USimFxSubsystem falls back to it)
            print('FX WARNING: no %s: the colour flipbook is left out' % f)
    import_files(files, TEX)
    data = set(DATA_TEXTURES) | set(RECORDED_DATA)
    for name in names:
        path = TEX + '/' + name
        tex = unreal.load_asset(path)
        if tex is None:
            fail('import of %s.png made no texture' % name)
            continue
        setp(tex, 'srgb', name not in data)
        comp = COMPRESSION.get(name, 'TC_DEFAULT')
        setp(tex, 'compression_settings', getattr(unreal.TextureCompressionSettings, comp))
        setp(tex, 'lod_group', unreal.TextureGroup.TEXTUREGROUP_EFFECTS, required=False)
        # Always resident and trilinear (D1): a burst seen for the first time draws from its top mip, not from a
        # coarse one still streaming in, whose bilinear magnification made the radial glows polygons.
        setp(tex, 'never_stream', True)
        setp(tex, 'filter', unreal.TextureFilter.TF_TRILINEAR)
        if name in MAX_SIZE:
            setp(tex, 'max_texture_size', MAX_SIZE[name])
        if name in CLAMPED_NO_MIPS:
            setp(tex, 'address_x', unreal.TextureAddress.TA_CLAMP)
            setp(tex, 'address_y', unreal.TextureAddress.TA_CLAMP)
            setp(tex, 'mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        if not lib.save_asset(path, only_if_is_dirty=False):
            fail('could not save ' + path)
        made(path)


def import_sounds(repo):
    lib = unreal.EditorAssetLibrary
    sounds_dir = os.path.join(repo, 'art', 'fx', 'sounds')
    files = sorted(glob.glob(os.path.join(sounds_dir, '*.wav')))
    if not files:
        fail('no art/fx/sounds/*.wav (run tools/fx_sounds.py)')
        return
    names = [os.path.splitext(os.path.basename(f))[0] for f in files]
    loops, from_manifest = loop_names(sounds_dir, names)
    if not from_manifest:
        print('FX WARNING: no art/fx/sounds/sounds.json: loops chosen by name')
    import_files(files, SND)
    for name in names:
        path = SND + '/' + name
        wave = unreal.load_asset(path)
        if wave is None:
            fail('import of %s.wav made no sound' % name)
            continue
        setp(wave, 'looping', name in loops)
        if not lib.save_asset(path, only_if_is_dirty=False):
            fail('could not save ' + path)
        made(path + (' (loop)' if name in loops else ''))


# ---------------------------------------------------------------- materials
class Graph:
    """One material's graph being built: expression and link helpers in the style of ue_make_crt_material.py."""

    def __init__(self, m):
        self.m = m
        self.mel = unreal.MaterialEditingLibrary

    def expr(self, cls, x, y, **props):
        e = self.mel.create_material_expression(self.m, cls, x, y)
        if e is None:
            fail('%s: could not create %s' % (self.m.get_name(), cls.__name__))
            return None
        for k, v in props.items():
            setp(e, k, v)
        return e

    def link(self, a, aout, b, bin_):
        if a is None or b is None or not self.mel.connect_material_expressions(a, aout, b, bin_):
            names = ''
            if a is not None and b is not None:
                names = ' (outputs %s, inputs %s)' % (list(self.mel.get_material_expression_output_names(a)),
                                                      list(self.mel.get_material_expression_input_names(b)))
            fail('%s: LINK FAILED %s %r -> %s %r%s' % (self.m.get_name(), a.get_name() if a else None, aout,
                                                       b.get_name() if b else None, bin_, names))

    def out(self, e, eout, prop):
        if e is None or not self.mel.connect_material_property(e, eout, prop):
            fail('%s: could not connect %s to %s' % (self.m.get_name(), e.get_name() if e else None, prop))

    # nodes
    def scalar(self, name, default, x, y):
        return self.expr(unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=default)

    def vector(self, name, rgb, x, y):
        return self.expr(unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name,
                         default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))

    def const(self, v, x, y):
        return self.expr(unreal.MaterialExpressionConstant, x, y, r=v)

    def texture(self, name, asset, colour, uv, x, y, optional=None):
        """A texture parameter. optional: a stand-in asset used (with a warning) when this one is missing."""
        tex = unreal.load_asset(TEX + '/' + asset)
        if tex is None and optional is not None:
            print('FX WARNING: %s: no texture %s, %s stands in' % (self.m.get_name(), asset, optional))
            asset = optional
            tex = unreal.load_asset(TEX + '/' + asset)
        if tex is None:
            fail('%s: no texture %s' % (self.m.get_name(), asset))
            tex = unreal.load_asset('/Engine/EngineResources/DefaultTexture')
        if COMPRESSION.get(asset) == 'TC_GRAYSCALE':
            st = unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE
        elif colour:
            st = unreal.MaterialSamplerType.SAMPLERTYPE_COLOR
        else:
            st = unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR
        t = self.expr(unreal.MaterialExpressionTextureSampleParameter2D, x, y, parameter_name=name, texture=tex,
                      sampler_type=st)
        if uv is not None:
            self.link(uv, '', t, 'UVs')
        return t

    def binary(self, cls, a, b, x, y, aout='', bout='', **props):
        e = self.expr(cls, x, y, **props)
        if a is not None:
            self.link(a, aout, e, 'A')
        if b is not None:
            self.link(b, bout, e, 'B')
        return e

    def mul(self, a, b, x, y, aout='', bout='', **props):
        return self.binary(unreal.MaterialExpressionMultiply, a, b, x, y, aout, bout, **props)

    def add(self, a, b, x, y, aout='', bout='', **props):
        return self.binary(unreal.MaterialExpressionAdd, a, b, x, y, aout, bout, **props)

    def append(self, a, b, x, y, aout='', bout=''):
        return self.binary(unreal.MaterialExpressionAppendVector, a, b, x, y, aout, bout)

    def sub(self, a, b, x, y, aout='', bout='', **props):
        return self.binary(unreal.MaterialExpressionSubtract, a, b, x, y, aout, bout, **props)

    def div(self, a, b, x, y, aout='', bout='', **props):
        return self.binary(unreal.MaterialExpressionDivide, a, b, x, y, aout, bout, **props)

    def lerp(self, a, b, alpha, x, y, aout='', bout='', alphaout=''):
        e = self.expr(unreal.MaterialExpressionLinearInterpolate, x, y)
        self.link(a, aout, e, 'A')
        self.link(b, bout, e, 'B')
        self.link(alpha, alphaout, e, 'Alpha')
        return e

    def sat(self, a, x, y, aout=''):
        return self.unary(unreal.MaterialExpressionSaturate, a, x, y, aout)

    def power(self, base, exponent, x, y, baseout='', expout=''):
        e = self.expr(unreal.MaterialExpressionPower, x, y)
        self.link(base, baseout, e, 'Base')
        self.link(exponent, expout, e, 'Exp')   # the pin's name (the property is Exponent)
        return e

    def smoothstep(self, lo, hi, value, x, y, valueout=''):
        e = self.expr(unreal.MaterialExpressionSmoothStep, x, y)
        self.link(lo, '', e, 'Min')
        self.link(hi, '', e, 'Max')
        self.link(value, valueout, e, 'Value')
        return e

    def link_any(self, a, aout, b, names):
        """Connect to the first of the input names that takes it (pin names differ from property names)."""
        for n in names:
            if a is not None and b is not None and self.mel.connect_material_expressions(a, aout, b, n):
                return
        self.link(a, aout, b, names[0])

    def unary(self, cls, a, x, y, aout=''):
        e = self.expr(cls, x, y)
        self.link(a, aout, e, '')
        return e

    def custom_data(self, x, y):
        """The eight per-instance floats, by name; 'tint' is the three appended."""
        d = {}
        for i, (index, name, default) in enumerate(CUSTOM_DATA):
            d[name] = self.expr(unreal.MaterialExpressionPerInstanceCustomData, x, y + 90 * i, data_index=index,
                                const_default_value=default)
        rg = self.append(d['tint_r'], d['tint_g'], x + 250, y + 300)
        d['tint'] = self.append(rg, d['tint_b'], x + 400, y + 330)
        lrg = self.append(d['local_r'], d['local_g'], x + 250, y + 900)
        d['local'] = self.append(lrg, d['local_b'], x + 400, y + 930)
        return d

    def flipbook_uv(self, frame, cols, rows, x, y):
        """cell = floor(frame); col = fmod(cell, Cols); row = floor(cell / Cols); uv = (uv0 + (col, row)) / (Cols, Rows)."""
        cell = self.unary(unreal.MaterialExpressionFloor, frame, x, y)
        col = self.binary(unreal.MaterialExpressionFmod, cell, cols, x + 160, y)
        q = self.binary(unreal.MaterialExpressionDivide, cell, cols, x + 160, y + 110)
        row = self.unary(unreal.MaterialExpressionFloor, q, x + 320, y + 110)
        offset = self.append(col, row, x + 480, y)
        tc = self.expr(unreal.MaterialExpressionTextureCoordinate, x + 480, y - 130)
        shifted = self.add(tc, offset, x + 640, y)
        dims = self.append(cols, rows, x + 480, y + 230)
        return self.binary(unreal.MaterialExpressionDivide, shifted, dims, x + 800, y)

    def erosion_opacity(self, s, d, x, y):
        """A opacity smoothstep(erosion, erosion + 0.15, B): the puff eats away from its thin parts as it ages."""
        top = self.add(d['erosion'], None, x, y + 120, const_b=0.15)
        ss = self.expr(unreal.MaterialExpressionSmoothStep, x + 160, y + 60)
        self.link(d['erosion'], '', ss, 'Min')
        self.link(top, '', ss, 'Max')
        self.link(s, 'B', ss, 'Value')
        a = self.mul(s, d['opacity'], x + 160, y - 60, aout='A')
        return self.mul(a, ss, x + 320, y)


def new_material(name):
    lib, mel = unreal.EditorAssetLibrary, unreal.MaterialEditingLibrary
    path = MAT + '/' + name
    if lib.does_asset_exist(path):
        m = unreal.load_asset(path)
        if not isinstance(m, unreal.Material):
            fail('%s exists and is not a material' % path)
            return None
        mel.delete_all_material_expressions(m)
    else:
        m = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, MAT, unreal.Material, unreal.MaterialFactoryNew())
    if m is None:
        fail('could not make ' + path)
    return m


def sprite_settings(m, blend):
    """Unlit, two-sided, drawn by instanced static meshes, no depth written, fogged."""
    setp(m, 'material_domain', unreal.MaterialDomain.MD_SURFACE)
    setp(m, 'blend_mode', blend)
    setp(m, 'shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    setp(m, 'two_sided', True)
    setp(m, 'use_translucency_vertex_fog', True)    # "Apply Fogging"
    setp(m, 'compute_fog_per_pixel', True)          # D4: fog at the quad's corners made a seam along its diagonal
    setp(m, 'output_translucent_velocity', False)   # "Output Depth and Velocity": off, so no depth is written
    unreal.MaterialEditingLibrary.set_base_material_usage(m, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES, True)
    if not m.get_editor_property('used_with_instanced_static_meshes'):
        fail('%s: not marked for instanced static meshes' % m.get_name())


def finish(m):
    lib, mel = unreal.EditorAssetLibrary, unreal.MaterialEditingLibrary
    errors = mel.recompile_material(m)
    for e in errors or []:
        fail('%s: compile: %s' % (m.get_name(), e))
    if not lib.save_asset(m.get_path_name(), only_if_is_dirty=False):
        fail('could not save ' + m.get_path_name())
    made(m.get_path_name())


def make_smoke():
    """Smoke, debris and fire in one premultiplied (AlphaComposite) draw, sorted back to front together (T5, the art
    review's D6): soot hides the fireball behind it, a fireball in front glows over the smoke. Float 8 picks the
    sprite's picture: 0 the smoke (or debris) atlas, 1 the generated fireball flipbook, 2 the colour flipbook."""
    m = new_material('M_FX_Smoke')
    if m is None:
        return
    sprite_settings(m, unreal.BlendMode.BLEND_ALPHA_COMPOSITE)
    g = Graph(m)
    d = g.custom_data(-3600, -300)
    sun = g.vector('SunColor', (1.0, 0.96, 0.88), -1600, -900)
    amb_p = g.vector('AmbientColor', (0.42, 0.47, 0.55), -1600, -1050)
    bright = g.scalar('Brightness', 1.0, -1600, -1150)
    firec = g.vector('FireColor', (1.0, 0.45, 0.15), -1600, -1250)
    scale = g.scalar('EmissiveScale', 20.0, -1600, -1350)
    colour_glow = g.scalar('ColourGlow', 0.1, -1600, -1450)
    one = g.const(1.0, -3000, 1400)

    # The kind masks (kind is 0, 1 or 2).
    k = d['kind']
    m0 = g.sat(g.sub(one, k, -3000, 1600), -2850, 1600)
    k1 = g.unary(unreal.MaterialExpressionAbs, g.sub(k, one, -3000, 1700), -2850, 1700)
    m1 = g.sat(g.sub(one, k1, -2700, 1700), -2550, 1700)
    m2 = g.sat(g.sub(k, one, -3000, 1800), -2850, 1800)

    # The light on a white surface: the sky, the sun, the flashes and fires near it (T9). The sun by pixel on a sphere
    # (the second art review's item 1: lit as one flat card, a front-lit puff clipped to white with no underside):
    # N = normalize(p + 0.6 CameraVector), p the pixel's offset from the sprite's centre over its radius (radius =
    # fade_cm / 0.6: float 12 is 0.3 x the width), at most 1; the sun's share saturate(0.3 + 0.7 N.SunDir), a quarter
    # of it the whole sprite's (float 7: thin smoke against the sun still glows).
    sundir = g.vector('SunDir', (0.0, 0.0, 1.0), -2600, -1650)
    # Both camera relative (translated world space): plain floats, no large-world-coordinate arithmetic.
    wpos = g.expr(unreal.MaterialExpressionWorldPosition, -3000, -1500,
                  world_position_shader_offset=enum_value(unreal.WorldPositionIncludedOffsets,
                                                          ('WPT_CAMERA_RELATIVE', 'CAMERA_RELATIVE')))
    opos = g.expr(unreal.MaterialExpressionObjectPositionWS, -3000, -1400,
                  origin_type=enum_value(unreal.PositionOrigin, ('CAMERA_RELATIVE',)))
    radius = g.mul(d['fade_cm'], None, -2850, -1300, const_b=1.0 / 0.6)
    off = g.div(g.sub(wpos, opos, -2850, -1450), radius, -2700, -1400)
    off_len = g.expr(unreal.MaterialExpressionLength, -2550, -1350)
    g.link(off, '', off_len, '')
    p_unit = g.div(off, g.binary(unreal.MaterialExpressionMax, off_len, one, -2400, -1350), -2250, -1400)
    camv = g.expr(unreal.MaterialExpressionCameraVectorWS, -2400, -1250)
    nrm = g.unary(unreal.MaterialExpressionNormalize, g.add(p_unit, g.mul(camv, None, -2250, -1250, const_b=0.6),
                                                            -2100, -1350), -1950, -1350)
    ndl = g.expr(unreal.MaterialExpressionDotProduct, -1800, -1400)
    g.link(nrm, '', ndl, 'A')
    g.link(sundir, '', ndl, 'B')
    shade = g.sat(g.add(g.mul(ndl, None, -1650, -1400, const_b=0.7), None, -1500, -1400, const_b=0.3), -1350, -1400)
    share = g.add(g.mul(shade, None, -1200, -1400, const_b=0.75), g.mul(d['lit'], None, -1200, -1300, const_b=0.25),
                  -1050, -1350)
    sun_l = g.mul(sun, share, -1400, -900)
    # The sky by the same normal (SimFxLogic::SkyShare): all of it on a top, SkyLow of it on an underside, which sees the
    # ground (the second art review's item 1: the sky lit the undersides as fully as the tops).
    up = g.expr(unreal.MaterialExpressionConstant3Vector, -1950, -1550, constant=unreal.LinearColor(0.0, 0.0, 1.0, 0.0))
    ndu = g.expr(unreal.MaterialExpressionDotProduct, -1800, -1550)
    g.link(nrm, '', ndu, 'A')
    g.link(up, '', ndu, 'B')
    up01 = g.sat(g.add(g.mul(ndu, None, -1650, -1550, const_b=0.5), None, -1500, -1550, const_b=0.5), -1350, -1550)
    sky_low = g.scalar('SkyLow', 0.55, -1500, -1650)
    sky_k = g.lerp(sky_low, one, up01, -1200, -1600)
    amb = g.mul(amb_p, sky_k, -1050, -1100)
    erosion = d['erosion']
    eros_top = g.add(erosion, None, -2400, 1200, const_b=0.15)

    # 0: the atlas. R lit from above, G thinness, B erosion field, A density.
    uv0 = g.flipbook_uv(d['frame'], g.scalar('Cols', 4.0, -3600, 900), g.scalar('Rows', 4.0, -3600, 1000), -3300, 800)
    s0 = g.texture('Atlas', 'T_FX_SmokeAtlas', False, uv0, -2400, 800)
    a0 = g.mul(g.smoothstep(erosion, eros_top, s0, -2200, 1000, valueout='B'), s0, -2050, 900, bout='A')
    light0 = g.add(g.add(amb, g.mul(sun_l, s0, -1200, -800, bout='R'), -1050, -850), d['local'], -900, -850)
    lit0 = g.mul(g.mul(d['tint'], light0, -750, -850), bright, -600, -850)
    heat0 = g.mul(g.mul(firec, d['heat'], -1200, -600), g.mul(s0, None, -1200, -500, aout='A', const_b=4.0), -1050, -600)
    e0 = g.mul(g.add(lit0, heat0, -450, -800), a0, -300, -800)

    # 1: the generated fireball. R temperature, G lit, B erosion, A density; HeatRamp colours it; the glow rises as
    # temp^4 (the art review's D2: a cooling fireball goes dark, it does not stay a bright red).
    uv1 = g.flipbook_uv(d['frame'], g.scalar('FlipCols', 8.0, -3600, 300), g.scalar('FlipRows', 8.0, -3600, 400), -3300, 200)
    s1 = g.texture('Flipbook', 'T_FX_Fireball', False, uv1, -2400, 200)
    a1 = g.mul(g.smoothstep(erosion, eros_top, s1, -2200, 400, valueout='B'), s1, -2050, 300, bout='A')
    temp = g.sat(g.mul(s1, d['heat'], -2000, 100, aout='R'), -1850, 100)
    ramp = g.texture('HeatRamp', 'T_FX_HeatRamp', True, g.append(temp, g.const(0.5, -1850, 200), -1700, 150), -1550, 100)
    t4 = g.power(temp, g.const(4.0, -1550, 300), -1400, 250)
    glow1 = g.mul(g.mul(ramp, scale, -1250, 100, aout='RGB'), t4, -1100, 150)
    light1 = g.add(g.add(amb, g.mul(sun_l, s1, -1200, 0, bout='G'), -1050, 0), d['local'], -900, 0)
    lit1 = g.mul(g.mul(d['tint'], light1, -750, 0), bright, -600, 0)
    e1 = g.mul(g.add(lit1, glow1, -450, 50), a1, -300, 50)

    # 2: the colour flipbook (straight alpha, the light baked in) and its HDR brightness, log encoded: L = 161^v - 1.
    # Where L is high the picture is fire: its glow is the picture's colour times L, by temp^4 (heat), and what is
    # left there once it cools is soot of the sprite's tint; elsewhere the picture is smoke, lit by our light.
    uv2 = g.flipbook_uv(d['frame'], g.scalar('ColourCols', 5.0, -3600, -300), g.scalar('ColourRows', 5.0, -3600, -200),
                        -3300, -400)
    c2 = g.texture('ColourFlipbook', 'T_FXR_Unity_Explosion02HD_5x5', True, uv2, -2400, -400, optional='T_FX_HeatRamp')
    v2 = g.texture('EmissionLog', 'T_FXR_Unity_Explosion02HD_5x5_EmissionLog', False, uv2, -2400, -200,
                   optional='T_FX_Glow')
    lum = g.sub(g.power(g.const(161.0, -2200, -150), v2, -2050, -200, expout='R'), one, -1900, -200)
    w2 = g.sat(g.mul(g.sub(lum, g.const(0.6, -1900, -100), -1750, -150), None, -1600, -150, const_b=1.0 / 1.4), -1450, -150)
    a2 = g.mul(c2, g.smoothstep(erosion, g.add(erosion, None, -2200, -50, const_b=0.2), c2, -2050, -50, valueout='A'),
               -1900, -50, aout='A')
    albedo2 = g.lerp(c2, d['tint'], w2, -1300, -300, aout='RGB')
    light2 = g.add(g.add(amb, sun_l, -1200, -250), d['local'], -1050, -250)
    lit2 = g.mul(g.mul(albedo2, light2, -900, -300), bright, -750, -300)
    h4 = g.power(g.sat(d['heat'], -1600, -500), g.const(4.0, -1600, -450), -1450, -500)
    glow2 = g.mul(g.mul(g.mul(c2, lum, -1300, -550, aout='RGB'), w2, -1150, -550),
                  g.mul(g.mul(h4, scale, -1300, -450), colour_glow, -1150, -450), -1000, -500)
    e2 = g.mul(g.add(lit2, glow2, -600, -400), a2, -450, -400)

    # Mixed by kind, then the whole sprite's fades: its opacity (float 1), soft where it meets the ground (DepthFade
    # over float 12, the art review's D5) and gone within 1-4 m of the camera (the player stands in launch smoke).
    emix = g.add(g.add(g.mul(e0, m0, -150, -800), g.mul(e1, m1, -150, 50), 0, -400), g.mul(e2, m2, -150, -400), 150, -400)
    amix = g.add(g.add(g.mul(a0, m0, -150, 900), g.mul(a1, m1, -150, 1000), 0, 950), g.mul(a2, m2, -150, 1100), 150, 950)
    depth = g.expr(unreal.MaterialExpressionDepthFade, -300, 1300)
    g.link_any(one, '', depth, ['Opacity', 'InOpacity'])
    g.link_any(d['fade_cm'], '', depth, ['FadeDistance', 'Fade Distance'])
    pix = g.expr(unreal.MaterialExpressionPixelDepth, -450, 1450)
    cam = g.sat(g.mul(g.sub(pix, g.const(100.0, -450, 1550), -300, 1450), None, -150, 1450, const_b=1.0 / 300.0), 0, 1450)
    fade = g.mul(g.mul(d['opacity'], depth, 0, 1300), cam, 150, 1350)
    emissive = g.mul(emix, fade, 300, -300)
    opacity = g.mul(amix, fade, 300, 1000)
    g.out(emissive, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.out(opacity, '', unreal.MaterialProperty.MP_OPACITY)
    finish(m)


def make_additive():
    m = new_material('M_FX_Additive')
    if m is None:
        return
    sprite_settings(m, unreal.BlendMode.BLEND_ADDITIVE)
    g = Graph(m)
    d = g.custom_data(-2400, -300)
    cols, rows = g.scalar('Cols', 1.0, -2400, 600), g.scalar('Rows', 1.0, -2400, 700)
    intensity = g.scalar('Intensity', 10.0, -700, -400)
    uv = g.flipbook_uv(d['frame'], cols, rows, -2100, 500)
    s = g.texture('Tex', 'T_FX_Glow', False, uv, -1200, 100)
    # Kind 3: the detonation's spiky flash instead of the layer's own picture.
    fl = g.texture('Flash', 'T_FX_Flash', False, uv, -1200, 400, optional='T_FX_Glow')
    spiky = g.sat(g.add(d['kind'], None, -1200, 650, const_b=-2.0), -1050, 650)
    v0 = g.lerp(s, fl, spiky, -1000, 200, aout='R', bout='R')
    # Kind 4: an afterburner's flame (a nozzle-wide column, not the rocket plume's cone).
    ab = g.texture('Afterburner', 'T_FX_Afterburner', False, uv, -1200, 800, optional='T_FX_Plume')
    jet = g.sat(g.add(d['kind'], None, -1200, 1050, const_b=-3.0), -1050, 1050)
    v = g.lerp(v0, ab, jet, -900, 400, bout='R')
    # Emissive = v * tint * opacity * Intensity * (1 + 4 * heat)
    rt = g.mul(v, d['tint'], -900, -100)
    ro = g.mul(rt, d['opacity'], -750, -100)
    ri = g.mul(ro, intensity, -600, -150)
    h4 = g.mul(d['heat'], None, -900, 150, const_b=4.0)
    h1 = g.add(h4, None, -750, 150, const_b=1.0)
    emissive = g.mul(ri, h1, -450, -50)
    g.out(emissive, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m)


def make_ribbon():
    m = new_material('M_FX_Ribbon')
    if m is None:
        return
    setp(m, 'material_domain', unreal.MaterialDomain.MD_SURFACE)
    setp(m, 'blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    setp(m, 'shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    setp(m, 'two_sided', True)
    setp(m, 'use_translucency_vertex_fog', True)
    setp(m, 'compute_fog_per_pixel', True)
    setp(m, 'output_translucent_velocity', False)
    g = Graph(m)
    vc = g.expr(unreal.MaterialExpressionVertexColor, -900, -200)
    bright = g.scalar('Brightness', 1.0, -900, -350)
    uscale = g.scalar('UScale', 1.0, -1500, 200)
    one = g.const(1.0, -1500, 300)
    scale2 = g.append(uscale, one, -1350, 250)
    tc = g.expr(unreal.MaterialExpressionTextureCoordinate, -1350, 100, coordinate_index=0)
    uv = g.mul(tc, scale2, -1200, 150)
    t = g.texture('Tex', 'T_FX_Contrail', False, uv, -1050, 100)
    # The billows' shading over the per-vertex light (the critic's R2), then the flashes and fires near it (T9).
    shade = g.lerp(g.const(0.55, -900, 0), one, t, -750, 0, alphaout='R')
    lit = g.mul(g.mul(vc, bright, -600, -250), shade, -450, -200)
    uv2 = g.expr(unreal.MaterialExpressionTextureCoordinate, -1050, -500, coordinate_index=2)
    uv3 = g.expr(unreal.MaterialExpressionTextureCoordinate, -1050, -400, coordinate_index=3)
    uv3r = g.expr(unreal.MaterialExpressionComponentMask, -900, -400, r=True, g=False, b=False, a=False)
    g.link(uv3, '', uv3r, '')
    local = g.append(uv2, uv3r, -750, -450)
    emissive = g.add(lit, local, -300, -250)
    # Eaten away from its thin parts as it ages (UV1.x), and a far hairline left unbroken (UV1.y).
    uv1 = g.expr(unreal.MaterialExpressionTextureCoordinate, -1050, 400, coordinate_index=1)
    ero = g.expr(unreal.MaterialExpressionComponentMask, -900, 400, r=True, g=False, b=False, a=False)
    g.link(uv1, '', ero, '')
    hair = g.expr(unreal.MaterialExpressionComponentMask, -900, 500, r=False, g=True, b=False, a=False)
    g.link(uv1, '', hair, '')
    ss = g.smoothstep(ero, g.add(ero, None, -750, 450, const_b=0.2), t, -600, 400, valueout='B')
    dens = g.mul(t, ss, -450, 300, aout='A')
    a = g.lerp(dens, g.const(0.8, -450, 450), hair, -300, 350)
    # Gone near the camera by the trail's own width (UV3.y, metres; LOOK 9.3 and T2: the player stands in the smoke of
    # his own launch): clear within 1 m plus its width, whole again 3 m plus three widths further on. Further out than
    # the sprites' fade (SimFxLogic::CameraFade): a flat strip close to the camera twists into sheets and wedges, and the
    # launch cloud's puffs stand there instead.
    wm = g.expr(unreal.MaterialExpressionComponentMask, -900, 650, r=False, g=True, b=False, a=False)
    g.link(uv3, '', wm, '')
    pix = g.expr(unreal.MaterialExpressionPixelDepth, -900, 750)
    near = g.add(g.mul(wm, None, -750, 650, const_b=100.0), None, -600, 650, const_b=100.0)
    span = g.add(g.mul(wm, None, -750, 750, const_b=300.0), None, -600, 750, const_b=300.0)
    cam = g.sat(g.div(g.sub(pix, near, -450, 700), span, -300, 700), -150, 700)
    opacity = g.mul(g.mul(vc, a, -150, 200, aout='A'), cam, 0, 300)
    g.out(emissive, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.out(opacity, '', unreal.MaterialProperty.MP_OPACITY)
    finish(m)


def make_munition():
    m = new_material('M_FX_Munition')
    if m is None:
        return
    setp(m, 'material_domain', unreal.MaterialDomain.MD_SURFACE)
    setp(m, 'blend_mode', unreal.BlendMode.BLEND_OPAQUE)
    setp(m, 'shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    g = Graph(m)
    vc = g.expr(unreal.MaterialExpressionVertexColor, -600, -200)
    rough = g.scalar('Roughness', 0.45, -600, 0)
    metal = g.scalar('Metallic', 0.1, -600, 100)
    spec = g.const(0.5, -600, 200)
    g.out(vc, '', unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    g.out(metal, '', unreal.MaterialProperty.MP_METALLIC)
    g.out(spec, '', unreal.MaterialProperty.MP_SPECULAR)
    finish(m)


def make_scorch():
    m = new_material('M_FX_Scorch')
    if m is None:
        return
    setp(m, 'material_domain', unreal.MaterialDomain.MD_DEFERRED_DECAL)
    setp(m, 'blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    # What the decal writes follows from its pins (colour, roughness); this flag is how surfaces take decals.
    setp(m, 'material_decal_response', unreal.MaterialDecalResponse.MDR_COLOR_ROUGHNESS, required=False)
    g = Graph(m)
    t = g.texture('Tex', 'T_FX_Scorch', True, None, -700, -100)
    op = g.scalar('Opacity', 1.0, -700, 150)
    rough = g.const(0.95, -400, 50)
    opacity = g.mul(t, op, -400, 150, aout='A')
    g.out(t, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    g.out(opacity, '', unreal.MaterialProperty.MP_OPACITY)
    finish(m)


def verify():
    lib = unreal.EditorAssetLibrary
    for path in [TEX + '/' + n for n in DATA_TEXTURES + COLOUR_TEXTURES] + [MAT + '/' + n for n in MATERIALS]:
        if not lib.does_asset_exist(path):
            fail('missing afterwards: ' + path)


def main():
    repo = repo_root()
    print('FX assets from ' + repo)
    for step in (import_textures, import_sounds):
        try:
            step(repo)
        except Exception as e:  # noqa: BLE001
            fail('%s: %s' % (step.__name__, e))
    for make in (make_smoke, make_additive, make_ribbon, make_munition, make_scorch):
        try:
            make()
        except Exception as e:  # noqa: BLE001
            fail('%s: %s' % (make.__name__, e))
    verify()
    if FAILURES:
        unreal.log_error('FX ASSETS FAILED: %d problem(s); first: %s' % (len(FAILURES), FAILURES[0]))
    else:
        print('FX ASSETS OK')


if unreal is not None:
    main()
