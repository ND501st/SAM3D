"""Create the Kesmark test-range level (/Game/Maps/KesmarkRange) and the map's vertex-colour material, without opening
the editor (docs/TEST_RANGE.md):

    D:/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe C:/dev/SAM3D/unreal/SAM3D/SAM3D.uproject ^
        -run=pythonscript -script="C:/dev/SAM3D/tools/ue_build_kesmark_range.py" -unattended -nullrhi

(Runs inside Unreal's Python; build the C++ module first.) The level holds no geometry of its own: an ASimMapActor
draws data/maps/kesmark_range.json at play time in the scenario's frame, an ASimSkyWeatherActor the sun, the sky and
the cloud from the scenario's weather, and a Sim Scenario Director (no Cesium) plays scenarios/kesmark_range_test.json
in the editor's Play. Rerunning replaces both assets.
"""
import unreal

LEVEL = '/Game/Maps/KesmarkRange'
MATERIAL_DIR = '/Game/SAM3D/Map'
MATERIAL = 'M_SimMapVertexColor'


def build_material():
    """Lit, rough, coloured by the mesh's vertex colours: the ground's fields."""
    path = MATERIAL_DIR + '/' + MATERIAL
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = tools.create_asset(MATERIAL, MATERIAL_DIR, unreal.Material, unreal.MaterialFactoryNew())
    lib = unreal.MaterialEditingLibrary
    vc = lib.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -400, 0)
    rough = lib.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 200)
    rough.set_editor_property('r', 0.95)
    spec = lib.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 300)
    spec.set_editor_property('r', 0.2)
    lib.connect_material_property(vc, '', unreal.MaterialProperty.MP_BASE_COLOR)  # the first output: RGB
    lib.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    lib.connect_material_property(spec, '', unreal.MaterialProperty.MP_SPECULAR)
    lib.recompile_material(mat)
    if lib.get_material_property_input_node(mat, unreal.MaterialProperty.MP_BASE_COLOR) is None:
        raise RuntimeError('the vertex colour did not connect to the base colour')
    unreal.EditorAssetLibrary.save_asset(path)
    unreal.log('KESMARK RANGE: material %s' % path)


def build_level():
    if unreal.EditorAssetLibrary.does_asset_exist(LEVEL):
        unreal.EditorAssetLibrary.delete_asset(LEVEL)
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not levels.new_level(LEVEL):
        raise RuntimeError('could not create ' + LEVEL)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    zero = unreal.Vector(0, 0, 0)
    director = actors.spawn_actor_from_class(unreal.SimScenarioDirector, zero)
    director.set_actor_label('SimScenarioDirector')
    director.set_editor_property('scenario_file', 'scenarios/kesmark_range_test.json')
    director.set_editor_property('use_cesium_georeference', False)
    mapper = actors.spawn_actor_from_class(unreal.SimMapActor, zero)
    mapper.set_actor_label('KesmarkRangeMap')
    mapper.set_editor_property('map_file', 'maps/kesmark_range.json')
    sky = actors.spawn_actor_from_class(unreal.SimSkyWeatherActor, zero)
    sky.set_actor_label('SkyWeather')
    start = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0, 0, 500))
    start.set_actor_label('PlayerStart')
    levels.save_current_level()
    unreal.log('KESMARK RANGE: level %s saved' % LEVEL)


build_material()
build_level()
