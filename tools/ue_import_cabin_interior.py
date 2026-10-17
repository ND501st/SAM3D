"""Import the 3D cabin's art into the open SAM3D editor: textures, materials and the interior meshes.

Needs the editor open with remote execution on (tools/ue_remote.py) and Play stopped (importing while Play runs can
crash the editor's mesh builder). Run the art builders first:

    "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" --background --python tools/blender/build_cabin_interior.py
    python tools/cabin_textures.py
    python tools/ue_import_cabin_interior.py              # everything
    python tools/ue_import_cabin_interior.py textures materials   # or some of: textures materials meshes
    python tools/ue_import_cabin_interior.py meshes --only SM_RadioR123M   # only the named meshes (comma-separated)

1. textures: art/cabin_textures/*.png -> /Game/SAM3D/Cabin/Textures (normal maps as normal maps, the grime masks
   linear, the panel faces sRGB colour).
2. materials: /Game/SAM3D/Cabin/Materials/M_CabinPBR, the cabin's lit master material (Color, Roughness, Metallic,
   EmissiveColor; a tiling detail normal map and grime masks on the box-projected UVs, 1 unit = 1 m; an optional
   BaseTexture multiplied in, for the panel faces), M_CabinGlass (thin tinted glass), and one instance per material
   slot of the interior meshes, MI_Cabin_<name>, plus MI_Cabin_PanelFace for the baked panel faces. Re-running
   rebuilds them in place (the instances keep their paths, so meshes keep their slots).
3. meshes: art/cabin_interior/SM_*.fbx -> /Game/SAM3D/Cabin/Interior, each slot bound to the MI_Cabin_<name> of the
   same name (the import creates no materials), no collision, Nanite off for glass.

The cabin (SimCabinComponent) places the meshes from the layout's shell parts of kind "mesh" and maps the panel
faces T_Panel_<panel> onto the console blocks.
"""
import glob
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ue_remote import Editor  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Each interior material: (linear base colour, roughness, metallic, grime 0..1, detail normal, normal strength,
# detail tiling per metre, emissive). Colours from the cabin photos (usaf_shaw_2017, opisybroni, rwd_mb3).
MATERIALS = {
    'Paint_Interior': ((0.16, 0.168, 0.15), 0.62, 0.0, 0.30, 'PaintN', 0.12, 3.0, None),  # grey-green lining
    'Paint_Rack': ((0.095, 0.1, 0.095), 0.55, 0.0, 0.30, 'PaintN', 0.12, 3.0, None),      # dark grey rack frames, posts
    'Paint_Bench': ((0.062, 0.076, 0.055), 0.62, 0.0, 0.55, 'PaintN', 0.45, 1.5, None),   # dark green bench
    'Paint_Equipment': ((0.105, 0.125, 0.075), 0.58, 0.0, 0.45, 'PaintN', 0.40, 2.0, None),
    'Paint_Radio': ((0.2, 0.212, 0.22), 0.46, 0.0, 0.3, 'PaintN', 0.45, 3.0, None),       # R-123M hammer grey (r123_kuzmin, r123m_qrz)
    'Paint_Panel': ((0.53, 0.52, 0.47), 0.5, 0.0, 0.2, 'PaintN', 0.2, 2.0, None),
    'Floor_Steel': ((0.14, 0.14, 0.13), 0.72, 0.15, 0.75, 'PaintN', 0.6, 1.0, None),
    'Rubber_Mat': ((0.018, 0.018, 0.018), 0.85, 0.0, 0.5, 'RibN', 1.0, 1.0, None),
    'Rubber_Black': ((0.012, 0.012, 0.012), 0.72, 0.0, 0.2, 'PaintN', 0.2, 3.0, None),
    'Vinyl_Olive': ((0.052, 0.054, 0.03), 0.5, 0.0, 0.4, 'VinylN', 0.7, 4.0, None),
    'Vinyl_Brown': ((0.06, 0.042, 0.03), 0.55, 0.0, 0.5, 'VinylN', 0.7, 4.0, None),
    'Metal_Steel': ((0.42, 0.42, 0.43), 0.33, 1.0, 0.25, 'PaintN', 0.1, 4.0, None),
    'Metal_Dark': ((0.03, 0.03, 0.03), 0.5, 0.0, 0.3, 'PaintN', 0.2, 3.0, None),
    'Bakelite_Black': ((0.012, 0.012, 0.012), 0.28, 0.0, 0.1, 'PaintN', 0.05, 4.0, None),
    'Lamp_Glass': ((0.8, 0.78, 0.72), 0.3, 0.0, 0.0, 'PaintN', 0.0, 1.0, (1.0, 0.82, 0.6, 6.0)),
    'Lamp_Blue': ((0.05, 0.12, 0.35), 0.25, 0.0, 0.0, 'PaintN', 0.0, 1.0, (0.12, 0.3, 1.0, 1.5)),
    'Duct_Grey': ((0.19, 0.19, 0.18), 0.78, 0.0, 0.5, 'PaintN', 0.3, 2.0, None),
    'Grate_Rubber': ((0.045, 0.024, 0.014), 0.8, 0.0, 0.5, 'PaintN', 0.3, 2.0, None),
    'Canvas': ((0.14, 0.13, 0.07), 0.9, 0.0, 0.5, 'VinylN', 0.4, 6.0, None),
    'Paint_Letters': ((0.5, 0.48, 0.42), 0.5, 0.0, 0.1, 'PaintN', 0.05, 4.0, None),   # engraved letters filled cream
    'Paint_Red': ((0.3, 0.035, 0.012), 0.35, 0.0, 0.05, 'PaintN', 0.05, 4.0, None),
    'Metal_Brass': ((0.42, 0.3, 0.12), 0.38, 1.0, 0.35, 'PaintN', 0.1, 4.0, None),   # brass screw heads (r123_kuzmin)
    'PanelFace': ((1.0, 1.0, 1.0), 0.5, 0.0, 0.0, 'PaintN', 0.18, 2.0, None),
}

EDITOR = r'''
import unreal, os, glob
lib = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
TEX = '/Game/SAM3D/Cabin/Textures'
MAT = '/Game/SAM3D/Cabin/Materials'
INT = '/Game/SAM3D/Cabin/Interior'
STEPS = %(steps)r
PLAIN = ('SM_RadioR123M', 'SM_IntercomA1', 'SM_IntercomA2', 'SM_IntercomA4')   # no Nanite: their details are mm thick
ONLY = %(only)r
DF_SCALE = {'SM_OsaHull': 8.0, 'SM_RackSL1M': 2.0, 'SM_RackSL2M': 2.0, 'SM_RackSL3M': 2.0, 'SM_RackSP1M': 2.0,
            'SM_RackSP2M': 2.0, 'SM_RackSP3M': 2.0, 'SM_OperatorBench': 2.0}
MATERIALS = %(materials)r


def import_files(files, dest, options=None):
    tasks = []
    for f in files:
        t = unreal.AssetImportTask()
        t.filename = f
        t.destination_path = dest
        t.automated = True
        t.replace_existing = True
        t.save = False
        if options is not None:
            t.options = options
        tasks.append(t)
    tools.import_asset_tasks(tasks)
    return tasks


def textures():
    files = sorted(glob.glob(os.path.join(%(repo)r, 'art', 'cabin_textures', '*.png')))
    import_files(files, TEX)
    for f in files:
        name = os.path.splitext(os.path.basename(f))[0]
        tex = unreal.load_asset(TEX + '/' + name)
        if tex is None:
            print('MISSING', name)
            continue
        if name.endswith('N'):
            tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
            tex.set_editor_property('srgb', False)
            tex.set_editor_property('flip_green_channel', True)   # baked green-up (OpenGL)
        elif name == 'T_Cabin_Grime':
            tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS)
            tex.set_editor_property('srgb', False)
        else:
            tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_DEFAULT)
            tex.set_editor_property('srgb', True)
        tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD)
        lib.save_asset(tex.get_path_name())
    print('textures', len(files))


def expr(m, cls, x, y, **props):
    e = mel.create_material_expression(m, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def link(a, aout, b, bin_):
    if not mel.connect_material_expressions(a, aout, b, bin_):
        print('LINK FAILED', a.get_name(), aout, '->', b.get_name(), bin_)


def new_material(name):
    path = MAT + '/' + name
    if lib.does_asset_exist(path):
        m = unreal.load_asset(path)
        mel.delete_all_material_expressions(m)
        return m
    return tools.create_asset(name, MAT, unreal.Material, unreal.MaterialFactoryNew())


def master():
    m = new_material('M_CabinPBR')
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
    m.set_editor_property('two_sided', False)
    uv = expr(m, unreal.MaterialExpressionTextureCoordinate, -1600, 0)
    s = unreal.MaterialExpressionScalarParameter
    v = unreal.MaterialExpressionVectorParameter
    colour = expr(m, v, -600, -500, parameter_name='Color', default_value=unreal.LinearColor(0.5, 0.5, 0.5, 1))
    rough = expr(m, s, -600, 250, parameter_name='Roughness', default_value=0.5)
    metal = expr(m, s, -600, 400, parameter_name='Metallic', default_value=0.0)
    grime_amt = expr(m, s, -1000, 600, parameter_name='GrimeAmount', default_value=0.3)
    grime_tile = expr(m, s, -1600, 300, parameter_name='GrimeTiling', default_value=0.6)
    detail_tile = expr(m, s, -1600, -300, parameter_name='DetailTiling', default_value=2.0)
    nstrength = expr(m, s, -900, -250, parameter_name='NormalStrength', default_value=0.3)
    emissive = expr(m, v, -600, 600, parameter_name='EmissiveColor', default_value=unreal.LinearColor(0, 0, 0, 1))
    # base texture (panel faces), default white
    base = expr(m, unreal.MaterialExpressionTextureSampleParameter2D, -1000, -700, parameter_name='BaseTexture',
                texture=unreal.load_asset('/Engine/EngineResources/WhiteSquareTexture'))
    link(uv, '', base, 'UVs')
    # detail normal
    duv = expr(m, unreal.MaterialExpressionMultiply, -1350, -250)
    link(uv, '', duv, 'A'); link(detail_tile, '', duv, 'B')
    dn = expr(m, unreal.MaterialExpressionTextureSampleParameter2D, -1150, -250, parameter_name='DetailNormal',
              texture=unreal.load_asset(TEX + '/T_Cabin_PaintN'), sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    link(duv, '', dn, 'UVs')
    flat = expr(m, unreal.MaterialExpressionConstant3Vector, -900, -100, constant=unreal.LinearColor(0, 0, 1, 1))
    nl = expr(m, unreal.MaterialExpressionLinearInterpolate, -700, -200)
    link(flat, '', nl, 'A'); link(dn, 'RGB', nl, 'B'); link(nstrength, '', nl, 'Alpha')
    # grime masks
    guv = expr(m, unreal.MaterialExpressionMultiply, -1350, 300)
    link(uv, '', guv, 'A'); link(grime_tile, '', guv, 'B')
    gt = expr(m, unreal.MaterialExpressionTextureSampleParameter2D, -1150, 300, parameter_name='Grime',
              texture=unreal.load_asset(TEX + '/T_Cabin_Grime'), sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    link(guv, '', gt, 'UVs')
    # g = saturate(R * 0.75 + G * 0.45 - 0.42) * GrimeAmount
    r75 = expr(m, unreal.MaterialExpressionMultiply, -950, 250, const_b=0.75)
    link(gt, 'R', r75, 'A')
    g45 = expr(m, unreal.MaterialExpressionMultiply, -950, 350, const_b=0.45)
    link(gt, 'G', g45, 'A')
    rg = expr(m, unreal.MaterialExpressionAdd, -820, 300)
    link(r75, '', rg, 'A'); link(g45, '', rg, 'B')
    off = expr(m, unreal.MaterialExpressionSubtract, -700, 300, const_b=0.42)
    link(rg, '', off, 'A')
    sat = expr(m, unreal.MaterialExpressionSaturate, -580, 300)
    mel.connect_material_expressions(off, '', sat, '')
    g = expr(m, unreal.MaterialExpressionMultiply, -460, 300)
    link(sat, '', g, 'A'); link(grime_amt, '', g, 'B')
    # base colour: Color * BaseTexture * (1 - 0.5 g) * (1 - 0.06 (B speck - 0.5))
    ctex = expr(m, unreal.MaterialExpressionMultiply, -400, -550)
    link(colour, '', ctex, 'A'); link(base, 'RGB', ctex, 'B')
    dark = expr(m, unreal.MaterialExpressionMultiply, -330, 150, const_b=-0.5)
    link(g, '', dark, 'A')
    one = expr(m, unreal.MaterialExpressionAdd, -220, 150, const_b=1.0)
    link(dark, '', one, 'A')
    bc = expr(m, unreal.MaterialExpressionMultiply, -150, -400)
    link(ctex, '', bc, 'A'); link(one, '', bc, 'B')
    # roughness: Roughness + 0.15 (G - 0.5) + 0.25 g
    gm = expr(m, unreal.MaterialExpressionSubtract, -950, 450, const_b=0.5)
    link(gt, 'G', gm, 'A')
    gm2 = expr(m, unreal.MaterialExpressionMultiply, -820, 450, const_b=0.15)
    link(gm, '', gm2, 'A')
    g25 = expr(m, unreal.MaterialExpressionMultiply, -330, 300, const_b=0.25)
    link(g, '', g25, 'A')
    r1 = expr(m, unreal.MaterialExpressionAdd, -400, 250)
    link(rough, '', r1, 'A'); link(gm2, '', r1, 'B')
    r2 = expr(m, unreal.MaterialExpressionAdd, -250, 250)
    link(r1, '', r2, 'A'); link(g25, '', r2, 'B')
    rs = expr(m, unreal.MaterialExpressionSaturate, -130, 250)
    mel.connect_material_expressions(r2, '', rs, '')
    mel.connect_material_property(bc, '', unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rs, '', unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(metal, '', unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(nl, '', unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(emissive, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    lib.save_asset(m.get_path_name())
    return m


def glass():
    m = new_material('M_CabinGlass')
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property('translucency_lighting_mode', unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    m.set_editor_property('two_sided', True)
    c = expr(m, unreal.MaterialExpressionVectorParameter, -500, -200, parameter_name='Color',
             default_value=unreal.LinearColor(0.04, 0.05, 0.05, 1))
    o = expr(m, unreal.MaterialExpressionScalarParameter, -500, 100, parameter_name='Opacity', default_value=0.14)
    r = expr(m, unreal.MaterialExpressionScalarParameter, -500, 0, parameter_name='Roughness', default_value=0.06)
    spec = expr(m, unreal.MaterialExpressionConstant, -500, 200, r=0.7)
    mel.connect_material_property(c, '', unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(o, '', unreal.MaterialProperty.MP_OPACITY)
    mel.connect_material_property(r, '', unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(spec, '', unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(m)
    lib.save_asset(m.get_path_name())
    return m


def instance(name, parent):
    path = MAT + '/' + name
    mi = unreal.load_asset(path) if lib.does_asset_exist(path) else tools.create_asset(
        name, MAT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    return mi


def materials():
    pbr = master()
    gl = glass()
    for name, (col, rough, metal, grime, normal, nstr, tiling, emis) in MATERIALS.items():
        mi = instance('MI_Cabin_' + name, pbr)
        mel.set_material_instance_vector_parameter_value(mi, 'Color', unreal.LinearColor(col[0], col[1], col[2], 1))
        mel.set_material_instance_scalar_parameter_value(mi, 'Roughness', rough)
        mel.set_material_instance_scalar_parameter_value(mi, 'Metallic', metal)
        mel.set_material_instance_scalar_parameter_value(mi, 'GrimeAmount', grime)
        mel.set_material_instance_scalar_parameter_value(mi, 'NormalStrength', nstr)
        mel.set_material_instance_scalar_parameter_value(mi, 'DetailTiling', tiling)
        mel.set_material_instance_texture_parameter_value(mi, 'DetailNormal', unreal.load_asset(TEX + '/T_Cabin_' + normal))
        if emis:
            mel.set_material_instance_vector_parameter_value(mi, 'EmissiveColor',
                unreal.LinearColor(emis[0] * emis[3], emis[1] * emis[3], emis[2] * emis[3], 1))
        mel.update_material_instance(mi)
        lib.save_asset(mi.get_path_name())
    g = instance('MI_Cabin_Glass', gl)
    mel.update_material_instance(g)
    lib.save_asset(g.get_path_name())
    print('materials', len(MATERIALS) + 3)


def meshes():
    files = sorted(glob.glob(os.path.join(%(repo)r, 'art', 'cabin_interior', 'SM_*.fbx')))
    if ONLY:
        files = [f for f in files if os.path.splitext(os.path.basename(f))[0] in ONLY]
    pipe = unreal.InterchangeGenericAssetsPipeline()
    mp = pipe.get_editor_property('material_pipeline')
    # no materials from the FBX: the slots are bound to the cabin's instances below, by name
    mp.set_editor_property('import_materials', False)
    common = pipe.get_editor_property('common_meshes_properties')
    for prop, value in (('force_all_mesh_as_type', unreal.InterchangeForceMeshType.IFMT_STATIC_MESH),):
        try:
            common.set_editor_property(prop, value)
        except Exception as e:
            print('skip', prop, e)
    mesh_pipe = pipe.get_editor_property('mesh_pipeline')
    # Nanite builds the fallback mesh LOD0 from a reduced copy: the radio's letters and screws would be gone from it. So
    # a run that imports only meshes that don't want Nanite builds none; a full run imports them with it, and the code
    # below turns it off for them (they keep their reduced fallback, fine for glass, wrong for the radio: import it alone).
    only_plain = bool(files) and all(os.path.splitext(os.path.basename(f))[0] in PLAIN for f in files)
    for prop, value in (('collision', False), ('build_nanite', not only_plain)):
        try:
            mesh_pipe.set_editor_property(prop, value)
        except Exception as e:
            print('skip', prop, e)
    tasks = import_files(files, INT, pipe)
    missing = []
    for f in files:
        name = os.path.splitext(os.path.basename(f))[0]
        sm = unreal.load_asset(INT + '/' + name)
        if sm is None:
            missing.append(name)
            continue
        # bind every slot to the MI of its name (in case the import left a default material)
        mats = sm.get_editor_property('static_materials')
        for i, sl in enumerate(mats):
            slot = str(sl.material_slot_name)
            mi = unreal.load_asset(MAT + '/' + slot) if lib.does_asset_exist(MAT + '/' + slot) else None
            if mi is not None:
                sm.set_material(i, mi)
            else:
                print('no material for slot', name, slot)
        if name in ('SM_OsaGlass',) + PLAIN:   # glass; and the radio and intercom boxes, whose letters and screws are millimetres thick: Nanite drops them
            ns = sm.get_editor_property('nanite_settings')
            ns.enabled = False
            sm.set_editor_property('nanite_settings', ns)
        # Lumen sees the cabin through the meshes' distance fields: the hull's 12 cm lining on a 6 m mesh needs a
        # finer one than the default, or the sky leaks in through the walls
        scale = DF_SCALE.get(name, 1.0)
        if scale != 1.0:
            sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
            bs = sub.get_lod_build_settings(sm, 0)
            bs.set_editor_property('distance_field_resolution_scale', scale)
            sub.set_lod_build_settings(sm, 0, bs)
        lib.save_asset(sm.get_path_name())
    print('meshes', len(files) - len(missing), 'missing', missing)


if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
    raise RuntimeError('stop Play first: importing while Play runs can crash the mesh builder')
for step in STEPS:
    globals()[step]()
'''


def main():
    args = sys.argv[1:]
    only = []
    if '--only' in args:
        i = args.index('--only')
        only = [n for n in args[i + 1].split(',') if n]
        args = args[:i] + args[i + 2:]
    steps = args or ['textures', 'materials', 'meshes']
    for s in steps:
        if s not in ('textures', 'materials', 'meshes'):
            sys.exit('unknown step ' + s)
    if 'meshes' in steps and not glob.glob(os.path.join(REPO, 'art', 'cabin_interior', 'SM_*.fbx')):
        sys.exit('no meshes: run tools/blender/build_cabin_interior.py first')
    with Editor() as ue:
        ok, out = ue.run(EDITOR % {'steps': steps, 'only': only, 'materials': MATERIALS, 'repo': REPO})
        print(out)
        if not ok:
            sys.exit(1)


if __name__ == '__main__':
    main()
