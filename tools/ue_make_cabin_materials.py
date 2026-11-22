"""Create the cabin's materials in the open SAM3D editor: /Game/SAM3D/Cabin/Materials/M_CabinSurface and M_CabinLabel.

Needs the editor open with remote execution on (tools/ue_remote.py). The material has three parameters the cabin sets
per part (SimCabinComponent): "Color" (base colour), "Roughness" and "Metallic", so one material gives glossy bakelite
knobs, polished chrome and brass, satin-painted block plates and the matt rack. Re-running rebuilds it.

    python tools/ue_make_cabin_materials.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ue_remote import Editor  # noqa: E402

MAKE = r'''
import unreal
lib = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
folder, name = '/Game/SAM3D/Cabin/Materials', 'M_CabinSurface'
path = folder + '/' + name
if lib.does_asset_exist(path):
    lib.delete_asset(path)
mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())

def param(cls, pname, default, x, y):
    e = mel.create_material_expression(mat, cls, x, y)
    e.set_editor_property('parameter_name', pname)
    e.set_editor_property('default_value', default)
    return e

color = param(unreal.MaterialExpressionVectorParameter, 'Color', unreal.LinearColor(0.5, 0.5, 0.5, 1.0), -500, -100)
rough = param(unreal.MaterialExpressionScalarParameter, 'Roughness', 0.4, -500, 150)
metal = param(unreal.MaterialExpressionScalarParameter, 'Metallic', 0.0, -500, 300)
mel.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
mel.connect_material_property(metal, '', unreal.MaterialProperty.MP_METALLIC)
mat.set_editor_property('used_with_nanite', True)  # the cabin's meshes are Nanite: without it the game draws the default
mel.recompile_material(mat)
lib.save_asset(path)
print('saved', path)

# Engraved labels: a texture's red channel as the opacity mask (white letters on black), the letters dark and lit
# like the plate they are on. The cabin draws each label into a small render target ("Text").
name = 'M_CabinLabel'
path = folder + '/' + name
if lib.does_asset_exist(path):
    lib.delete_asset(path)
lab = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
lab.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
lab.set_editor_property('two_sided', True)
tex = mel.create_material_expression(lab, unreal.MaterialExpressionTextureSampleParameter2D, -500, 0)
tex.set_editor_property('parameter_name', 'Text')
ink = mel.create_material_expression(lab, unreal.MaterialExpressionVectorParameter, -500, -250)
ink.set_editor_property('parameter_name', 'Color')
ink.set_editor_property('default_value', unreal.LinearColor(0.02, 0.02, 0.02, 1.0))
rough = mel.create_material_expression(lab, unreal.MaterialExpressionConstant, -500, 250)
rough.set_editor_property('r', 0.6)
mel.connect_material_property(ink, '', unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(tex, 'R', unreal.MaterialProperty.MP_OPACITY_MASK)
mel.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
mel.recompile_material(lab)
lib.save_asset(path)
print('saved', path)
'''


def main():
    with Editor() as ue:
        ok, out = ue.run(MAKE)
        print(out)
        if not ok:
            sys.exit(1)


if __name__ == '__main__':
    main()
