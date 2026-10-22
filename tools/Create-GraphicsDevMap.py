"""Graphics-menu review map /Game/Frontier/Maps/Dev/GraphicsSettings/GraphicsSettings_Review: a lit courtyard that shows the quality levels
(soft sun shadows from pillars, Lumen bounce off a red and a green wall, chrome spheres for reflections, a long avenue for view distance).
No world-settings override, so the project default ASFGameMode and ASFPlayerController run (any /Game/Frontier/Maps/Dev/ map is admitted).
Run inside the editor: py -3 tools/Run-InEditor.py tools/Create-GraphicsDevMap.py (refuses to overwrite the map)."""
import unreal

DIR = '/Game/Frontier/Maps/Dev/GraphicsSettings'
MAP = DIR + '/GraphicsSettings_Review'
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if unreal.EditorAssetLibrary.does_asset_exist(MAP):
    raise RuntimeError('Graphics review map already exists; refusing overwrite.')


def material(name, color, metallic=0.0, roughness=0.8):
    path = DIR + '/' + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, DIR, unreal.Material, unreal.MaterialFactoryNew())
    lib = unreal.MaterialEditingLibrary
    base = lib.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -400, 0)
    base.set_editor_property('constant', unreal.LinearColor(*color))
    lib.connect_material_property(base, '', unreal.MaterialProperty.MP_BASE_COLOR)
    met = lib.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 150)
    met.set_editor_property('r', metallic)
    lib.connect_material_property(met, '', unreal.MaterialProperty.MP_METALLIC)
    rough = lib.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 300)
    rough.set_editor_property('r', roughness)
    lib.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    lib.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    return mat


if not levels.new_level(MAP):
    raise RuntimeError('Could not create the graphics review map.')
cube = unreal.load_asset('/Engine/BasicShapes/Cube')
sphere = unreal.load_asset('/Engine/BasicShapes/Sphere')
cylinder = unreal.load_asset('/Engine/BasicShapes/Cylinder')
stone = material('M_GfxStone', (0.55, 0.52, 0.46))
red = material('M_GfxRed', (0.7, 0.05, 0.04))
green = material('M_GfxGreen', (0.06, 0.55, 0.08))
chrome = material('M_GfxChrome', (0.9, 0.9, 0.92), 1.0, 0.04)


def put(mesh, mat, label, loc, scale, rot=None):
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*loc), rot or unreal.Rotator())
    actor.set_actor_label(label)
    actor.static_mesh_component.set_static_mesh(mesh)
    actor.static_mesh_component.set_material(0, mat)
    actor.static_mesh_component.set_collision_profile_name('BlockAll')
    actor.set_actor_scale3d(unreal.Vector(*scale))
    return actor


put(cube, stone, 'Gfx_Floor', (0, 0, -50), (120, 40, 1))
put(cube, red, 'Gfx_WallRed', (-1000, 700, 250), (0.5, 4, 6))
put(cube, green, 'Gfx_WallGreen', (1000, 700, 250), (0.5, 4, 6))
put(cube, stone, 'Gfx_WallBack', (0, 1300, 300), (20, 0.5, 7))
for i, y in enumerate(range(-200, 4200, 700)):
    put(cylinder, stone, 'Gfx_PillarL%d' % i, (-350, y, 200), (1.2, 1.2, 4.5))
    put(cylinder, stone, 'Gfx_PillarR%d' % i, (350, y, 200), (1.2, 1.2, 4.5))
for i, (x, y) in enumerate([(-150, 500), (0, 700), (160, 480)]):
    put(sphere, chrome, 'Gfx_Chrome%d' % i, (x, y, 60), (1.1, 1.1, 1.1))
actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0, -600, 110), unreal.Rotator(yaw=90)).set_actor_label('Gfx_PlayerStart')
sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 1000), unreal.Rotator(pitch=-38, yaw=-50))
sun.set_actor_label('Gfx_Sun')
actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0)).set_actor_label('Gfx_SkyAtmosphere')
actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 500)).set_actor_label('Gfx_SkyLight')
actors.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0)).set_actor_label('Gfx_Fog')
if not levels.save_current_level():
    raise RuntimeError('Could not save the graphics review map.')
unreal.log('GRAPHICS_DEV_MAP_CREATED ' + MAP)
