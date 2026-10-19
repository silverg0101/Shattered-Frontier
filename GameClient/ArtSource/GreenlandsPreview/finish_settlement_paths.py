"""Join the final cottage doors to the preserved regional roads."""
import unreal as u
from pathlib import Path
ROOT=Path(__file__).resolve().parent;P='/Game/Frontier/Environment/GreenlandsPreview'
task=u.AssetImportTask();task.filename=str(ROOT/'PreparedArrival/SM_GreenlandsWornRoads.fbx');task.destination_path=P+'/Arrival/Meshes';task.automated=True;task.save=True;task.replace_existing=True
options=u.FbxImportUI();options.import_materials=False;options.import_textures=False;options.static_mesh_import_data.combine_meshes=True
options.static_mesh_import_data.set_editor_property('vertex_color_import_option',u.VertexColorImportOption.REPLACE);task.options=options
u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
mesh=u.load_asset(P+'/Arrival/Meshes/SM_GreenlandsWornRoads');mesh.set_material(0,u.load_asset(P+'/Arrival/Materials/M_WornTrack'));u.EditorAssetLibrary.save_loaded_asset(mesh)
u.get_editor_subsystem(u.LevelEditorSubsystem).load_level(P+'/Greenlands_EnvironmentPreview')
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
import sys
sys.path.insert(0,str(ROOT));from region_layout import height
for actor in actors.get_all_level_actors():
    if isinstance(actor,u.StaticMeshActor) and actor.static_mesh_component.static_mesh and actor.static_mesh_component.static_mesh.get_name()=='SM_LogStack':
        location=actor.get_actor_location()
        if abs(location.x+10500)<10 and abs(location.y+5700)<10:actor.set_actor_location(u.Vector(-11200,-6400,height(-112,-64)*100),False,False)
assert u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level()
