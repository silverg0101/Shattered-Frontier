"""Make the walking surface follow authored road crown and ruts."""
import unreal as u,json
from pathlib import Path
ROOT=Path(__file__).resolve().parent;P='/Game/Frontier/Environment/Greenlands'
mesh=u.load_asset(P+'/Arrival/Meshes/SM_GreenlandsWornRoads');mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE);u.EditorAssetLibrary.save_loaded_asset(mesh)
levels=u.get_editor_subsystem(u.LevelEditorSubsystem);levels.load_level('/Game/Frontier/Maps/Test/Greenlands_ArtReview')
for actor in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors():
    if isinstance(actor,u.StaticMeshActor) and actor.static_mesh_component.static_mesh==mesh:actor.static_mesh_component.set_collision_profile_name('BlockAll')
assert levels.save_current_level()
path=ROOT/'worn-road-finish.json';report=json.loads(path.read_text());report['physical_collision']='Complex road mesh collision follows crown and ruts above continuous terrain';path.write_text(json.dumps(report,indent=2))
