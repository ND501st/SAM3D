"""Import the map's ground and ribbon textures and build their materials (roadmap S9, docs/scenery/README.md "The
ground's and the ribbons' materials").

Imports art/map/textures/*.png (tools/map_textures.py) into /Game/SAM3D/Map/Textures, replacing what is there, and
builds /Game/SAM3D/Map/M_SimMapGround and M_SimMapRibbon (rebuilt from nothing when they exist). ASimMapActor loads them
by these names and falls back to the vertex colours (the ground) and the flat colours (the ribbons) when one is missing.

Headless, with the editor closed (from PowerShell):
    & 'D:\\UE_5.8\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe' C:\\dev\\SAM3D\\unreal\\SAM3D\\SAM3D.uproject `
        -run=pythonscript -script=C:\\dev\\SAM3D\\tools\\ue_make_map_materials.py -unattended -nullrhi
It prints one line per asset made and "MAP MATERIALS OK" at the end, or "MAP MATERIALS FAILED: <reason>".

The materials' contract (ASimMapActor::BuildGround, BuildStrips and BuildCountry are written to the same text):
  M_SimMapGround  opaque, lit. UV0 is the map's (east, north) in km; the vertex colour the ground's colour (linear), its
                  alpha the detail kind / 2 (0 a field, 0.5 grass and litter, 1 hard ground: simcore CoverDetail).
                  CoverTex (sRGB BGRA, one texel a 25 m cell of the land cover, row 0 the south) with CoverRect
                  (west_km, south_km, width_km, height_km) and UseCover 1 gives the colour and the detail kind (A) cell by
                  cell in place of the vertices'. m = UV0 1000; Detail = T_MapGroundDetail at m / DetailTileM and
                  m / (3.7 DetailTileM) + 0.37, averaged; v = lerp(lerp(R, G, sat(2 kind)), B, sat(2 kind - 1)),
                  faded to 0.5 from DetailFadeStartCm over DetailFadeLengthCm of pixel depth; macro = A at m / MacroTileM.
                  BaseColor = colour (1 - DetailContrast / 2 + DetailContrast v) (1 - MacroContrast / 2 + MacroContrast
                  macro); Normal T_MapGroundNormal at m / DetailTileM, flattened with the fade; Roughness, Specular.
  M_SimMapRibbon  opaque, lit. UV0 (along in units of 10 m, across 0 to 1). t = T_MapRibbon(UV0.x UScale, UV0.y);
                  d = dot(t, Channel) (R asphalt, G dirt, B railway, A water); BaseColor = Colour (0.55 + 0.9 d);
                  Roughness, Specular.
"""
import os

try:
    import unreal
    if not hasattr(unreal, 'EditorAssetLibrary'):
        unreal = None
except ImportError:  # imported by a test outside the editor: the tables only
    unreal = None

ROOT = '/Game/SAM3D/Map'
TEX = ROOT + '/Textures'
TEXTURES = ('T_MapGroundDetail', 'T_MapGroundNormal', 'T_MapRibbon')
NORMAL_TEXTURES = ('T_MapGroundNormal',)
MATERIALS = ('M_SimMapGround', 'M_SimMapRibbon')

# The ground material's parameters and their defaults (game values; tuned by eye with -SimMapShots).
GROUND_PARAMS = {
    'UseCover': 0.0, 'DetailTileM': 4.0, 'MacroTileM': 900.0, 'DetailContrast': 0.8, 'MacroContrast': 0.35,
    'DetailFadeStartCm': 30000.0, 'DetailFadeLengthCm': 200000.0, 'NormalStrength': 0.6, 'Roughness': 0.92,
    'Specular': 0.3,
}
RIBBON_PARAMS = {'UScale': 1.0, 'Roughness': 0.85, 'Specular': 0.4}

FAILURES = []


def repo_root():
    if unreal is not None:
        return os.path.normpath(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), '..', '..'))
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def fail(why):
    FAILURES.append(why)
    unreal.log_error('MAP MATERIALS ERROR: ' + why)


def setp(obj, name, value):
    try:
        obj.set_editor_property(name, value)
    except Exception as e:  # noqa: BLE001 - the editor raises its own types
        fail('%s.%s: %s' % (obj.get_name(), name, e))


def import_textures(repo):
    lib = unreal.EditorAssetLibrary
    tasks = []
    for name in TEXTURES:
        f = os.path.join(repo, 'art', 'map', 'textures', name + '.png')
        if not os.path.exists(f):
            fail('no art/map/textures/%s.png (run tools/map_textures.py)' % name)
            continue
        t = unreal.AssetImportTask()
        t.set_editor_property('filename', f)
        t.set_editor_property('destination_path', TEX)
        t.set_editor_property('destination_name', name)
        t.set_editor_property('replace_existing', True)
        t.set_editor_property('automated', True)
        t.set_editor_property('save', True)
        tasks.append(t)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    for name in TEXTURES:
        path = TEX + '/' + name
        tex = unreal.load_asset(path)
        if tex is None:
            fail('import of %s.png made no texture' % name)
            continue
        setp(tex, 'srgb', False)  # data: brightnesses and a normal, not colours
        if name in NORMAL_TEXTURES:
            setp(tex, 'compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
        else:
            setp(tex, 'compression_settings', unreal.TextureCompressionSettings.TC_BC7)
        setp(tex, 'lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD)
        setp(tex, 'filter', unreal.TextureFilter.TF_TRILINEAR)
        if not lib.save_asset(path, only_if_is_dirty=False):
            fail('could not save ' + path)
        print('made ' + path)


class Graph:
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
            fail('%s: LINK FAILED %s %r -> %s %r' % (self.m.get_name(), a.get_name() if a else None, aout, b.get_name() if b else None, bin_))

    def out(self, e, eout, prop):
        if e is None or not self.mel.connect_material_property(e, eout, prop):
            fail('%s: could not connect %s to %s' % (self.m.get_name(), e.get_name() if e else None, prop))

    def scalar(self, name, default, x, y):
        return self.expr(unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=default)

    def vector(self, name, rgba, x, y):
        return self.expr(unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name,
                         default_value=unreal.LinearColor(rgba[0], rgba[1], rgba[2], rgba[3]))

    def const(self, v, x, y):
        return self.expr(unreal.MaterialExpressionConstant, x, y, r=v)

    def sample(self, asset, sampler, uv, x, y, param=None):
        tex = unreal.load_asset(asset)
        if tex is None:
            fail('%s: no texture %s' % (self.m.get_name(), asset))
            tex = unreal.load_asset('/Engine/EngineResources/DefaultTexture')
        if param:
            t = self.expr(unreal.MaterialExpressionTextureSampleParameter2D, x, y, parameter_name=param, texture=tex, sampler_type=sampler)
        else:
            t = self.expr(unreal.MaterialExpressionTextureSample, x, y, texture=tex, sampler_type=sampler)
        if uv is not None:
            self.link(uv, '', t, 'UVs')
        return t

    def binary(self, cls, a, b, x, y, aout='', bout=''):
        e = self.expr(cls, x, y)
        self.link(a, aout, e, 'A')
        self.link(b, bout, e, 'B')
        return e

    def mul(self, a, b, x, y, aout='', bout=''):
        return self.binary(unreal.MaterialExpressionMultiply, a, b, x, y, aout, bout)

    def add(self, a, b, x, y, aout='', bout=''):
        return self.binary(unreal.MaterialExpressionAdd, a, b, x, y, aout, bout)

    def sub(self, a, b, x, y, aout='', bout=''):
        return self.binary(unreal.MaterialExpressionSubtract, a, b, x, y, aout, bout)

    def div(self, a, b, x, y, aout='', bout=''):
        return self.binary(unreal.MaterialExpressionDivide, a, b, x, y, aout, bout)

    def append(self, a, b, x, y, aout='', bout=''):
        return self.binary(unreal.MaterialExpressionAppendVector, a, b, x, y, aout, bout)

    def max(self, a, b, x, y, aout='', bout=''):
        return self.binary(unreal.MaterialExpressionMax, a, b, x, y, aout, bout)

    def lerp(self, a, b, alpha, x, y, aout='', bout='', alphaout=''):
        e = self.expr(unreal.MaterialExpressionLinearInterpolate, x, y)
        self.link(a, aout, e, 'A')
        self.link(b, bout, e, 'B')
        self.link(alpha, alphaout, e, 'Alpha')
        return e

    def mask(self, a, channel, x, y):
        e = self.expr(unreal.MaterialExpressionComponentMask, x, y, r=channel == 'R', g=channel == 'G', b=channel == 'B', a=channel == 'A')
        self.link(a, '', e, '')
        return e

    def sat(self, a, x, y, aout=''):
        e = self.expr(unreal.MaterialExpressionSaturate, x, y)
        self.link(a, aout, e, '')
        return e


def new_material(name):
    lib = unreal.EditorAssetLibrary
    path = ROOT + '/' + name
    if lib.does_asset_exist(path):
        m = unreal.load_asset(path)
        unreal.MaterialEditingLibrary.delete_all_material_expressions(m)
    else:
        m = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, ROOT, unreal.Material, unreal.MaterialFactoryNew())
    return m


def finish(m):
    unreal.MaterialEditingLibrary.recompile_material(m)
    if not unreal.EditorAssetLibrary.save_asset(m.get_path_name(), only_if_is_dirty=False):
        fail('could not save ' + m.get_path_name())
    print('made ' + m.get_path_name())


def build_ground():
    m = new_material('M_SimMapGround')
    g = Graph(m)
    S = unreal.MaterialSamplerType
    P = {k: g.scalar(k, v, -2400, -600 + 80 * i) for i, (k, v) in enumerate(GROUND_PARAMS.items())}
    tc = g.expr(unreal.MaterialExpressionTextureCoordinate, -2400, 400, coordinate_index=0)
    # The cover, cell by cell.
    rect = g.vector('CoverRect', (0.0, 0.0, 1.0, 1.0), -2400, 600)
    rg = g.append(rect, rect, -2200, 600, 'R', 'G')
    ba = g.append(rect, rect, -2200, 700, 'B', 'A')
    cuv = g.div(g.sub(tc, rg, -2050, 600), ba, -1900, 650)
    cover = g.sample('/Engine/EngineResources/DefaultTexture', S.SAMPLERTYPE_COLOR, cuv, -1700, 600, param='CoverTex')
    vc = g.expr(unreal.MaterialExpressionVertexColor, -1700, 400)
    colour = g.lerp(vc, cover, P['UseCover'], -1400, 400, '', '', '')
    kind = g.lerp(vc, cover, P['UseCover'], -1400, 520, 'A', 'A', '')
    # The detail, two scales averaged.
    metres = g.mul(tc, g.const(1000.0, -2300, 900), -2150, 900)
    uv1 = g.div(metres, P['DetailTileM'], -2000, 900)
    uv2 = g.add(g.div(metres, g.mul(P['DetailTileM'], g.const(3.7, -2150, 1050), -2000, 1050), -1850, 1000), g.const(0.37, -1850, 1100), -1700, 1050)
    detail = TEX + '/T_MapGroundDetail'
    d1 = g.sample(detail, S.SAMPLERTYPE_LINEAR_COLOR, uv1, -1500, 850)
    d2 = g.sample(detail, S.SAMPLERTYPE_LINEAR_COLOR, uv2, -1500, 1050)
    davg = g.mul(g.add(d1, d2, -1300, 950), g.const(0.5, -1300, 1050), -1150, 950)
    k2 = g.mul(kind, g.const(2.0, -1150, 600), -1000, 560)
    dr, dg, db = (g.mask(davg, c, -1000, 850 + 80 * i) for i, c in enumerate('RGB'))
    a = g.lerp(dr, dg, g.sat(k2, -850, 600), -700, 900)
    v = g.lerp(a, db, g.sat(g.sub(k2, g.const(1.0, -850, 700), -700, 650), -550, 650), -500, 900)
    depth = g.expr(unreal.MaterialExpressionPixelDepth, -1000, 1250)
    fade = g.sat(g.div(g.sub(depth, P['DetailFadeStartCm'], -850, 1250), P['DetailFadeLengthCm'], -700, 1250), -550, 1250)
    vf = g.lerp(v, g.const(0.5, -500, 1150), fade, -350, 1000)
    dc = P['DetailContrast']
    shade = g.add(g.mul(vf, dc, -200, 1000), g.sub(g.const(1.0, -350, 1150), g.mul(dc, g.const(0.5, -350, 1250), -200, 1250), -100, 1150), -50, 1000)
    macro_uv = g.div(metres, P['MacroTileM'], -2000, 1350)
    macro = g.sample(detail, S.SAMPLERTYPE_LINEAR_COLOR, macro_uv, -1500, 1350)
    mc = P['MacroContrast']
    mshade = g.add(g.mul(macro, mc, -1200, 1350, 'A', ''), g.sub(g.const(1.0, -1200, 1450), g.mul(mc, g.const(0.5, -1350, 1500), -1200, 1500), -1050, 1450), -900, 1400)
    base = g.mul(g.mul(colour, shade, 100, 400), mshade, 250, 400)
    g.out(base, '', unreal.MaterialProperty.MP_BASE_COLOR)
    # The normal, flattened with the fade and by its strength.
    nrm = g.sample(TEX + '/T_MapGroundNormal', S.SAMPLERTYPE_NORMAL, uv1, -1500, 1650)
    up = g.expr(unreal.MaterialExpressionConstant3Vector, -1300, 1800, constant=unreal.LinearColor(0.0, 0.0, 1.0, 1.0))
    flat = g.max(fade, g.sub(g.const(1.0, -1150, 1900), P['NormalStrength'], -1000, 1900), -850, 1850)
    n = g.lerp(nrm, up, flat, -650, 1700)
    g.out(n, '', unreal.MaterialProperty.MP_NORMAL)
    g.out(P['Roughness'], '', unreal.MaterialProperty.MP_ROUGHNESS)
    g.out(P['Specular'], '', unreal.MaterialProperty.MP_SPECULAR)
    finish(m)


def build_ribbon():
    m = new_material('M_SimMapRibbon')
    g = Graph(m)
    S = unreal.MaterialSamplerType
    P = {k: g.scalar(k, v, -1600, -300 + 80 * i) for i, (k, v) in enumerate(RIBBON_PARAMS.items())}
    colour = g.vector('Colour', (0.2, 0.2, 0.2, 1.0), -1600, 0)
    channel = g.vector('Channel', (1.0, 0.0, 0.0, 0.0), -1600, 150)
    tc = g.expr(unreal.MaterialExpressionTextureCoordinate, -1600, 350, coordinate_index=0)
    u = g.mul(g.mask(tc, 'R', -1500, 330), P['UScale'], -1400, 350)
    uv = g.append(u, g.mask(tc, 'G', -1500, 420), -1250, 350)
    t = g.sample(TEX + '/T_MapRibbon', S.SAMPLERTYPE_LINEAR_COLOR, uv, -1050, 350)
    ch4 = g.append(channel, channel, -1250, 150, '', 'A')
    d = g.binary(unreal.MaterialExpressionDotProduct, t, ch4, -850, 300, 'RGBA', '')
    shade = g.add(g.mul(d, g.const(0.9, -850, 450), -700, 350), g.const(0.55, -700, 450), -550, 380)
    base = g.mul(colour, shade, -350, 200)
    g.out(base, '', unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(P['Roughness'], '', unreal.MaterialProperty.MP_ROUGHNESS)
    g.out(P['Specular'], '', unreal.MaterialProperty.MP_SPECULAR)
    finish(m)


def fix_vertex_colour_usage():
    """M_SimMapVertexColor draws the procedural trees, masses and buildings (FSimMapAssets), instanced: without the
    usage flag the game draws the engine's default material on them ("missing usage flag InstancedStaticMeshes")."""
    lib = unreal.EditorAssetLibrary
    m = lib.load_asset(ROOT + '/M_SimMapVertexColor')
    if m is None:
        return
    if not m.get_editor_property('used_with_instanced_static_meshes'):
        m.set_editor_property('used_with_instanced_static_meshes', True)
        unreal.MaterialEditingLibrary.recompile_material(m)
        if not lib.save_loaded_asset(m, False):
            fail('M_SimMapVertexColor: not saved')
            return
        unreal.log('made ' + ROOT + '/M_SimMapVertexColor usable on instanced meshes')


def main():
    repo = repo_root()
    import_textures(repo)
    build_ground()
    build_ribbon()
    fix_vertex_colour_usage()
    if FAILURES:
        unreal.log_error('MAP MATERIALS FAILED: ' + '; '.join(FAILURES))
    else:
        unreal.log('MAP MATERIALS OK')


if unreal is not None:
    main()
