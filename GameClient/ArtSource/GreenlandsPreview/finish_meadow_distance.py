"""Retain whole-clump coverage through four real distance LODs."""
import unreal as u,json
from pathlib import Path
ROOT=Path(__file__).resolve().parent;P='/Game/Frontier/Environment/GreenlandsPreview';D=P+'/Arrival'
sub=u.get_editor_subsystem(u.StaticMeshEditorSubsystem)
assert sub,'LOD editing requires full editor mode: -ExecutePythonScript, not -run=pythonscript'
for i in range(4):
    mesh=u.load_asset(D+'/Meshes/SM_PH_GrassClump_%d'%i);assert mesh
    settings=u.EditorScriptingMeshReductionOptions();settings.auto_compute_lod_screen_size=False
    # A source single-stem card shrank the full clump at distance. Reduce the
    # complete geometry instead, preserving its actual physical footprint.
    sub.remove_lods(mesh)
    settings.reduction_settings=[u.EditorScriptingMeshReductionSettings(percent_triangles=p,screen_size=s) for p,s in [(1,1),(.35,.16),(.1,.055),(.04,.018)]]
    sub.set_lods(mesh,settings);assert mesh.get_num_lods()==4
    assert u.EditorAssetLibrary.save_loaded_asset(mesh)
tree=u.load_asset(D+'/Meshes/SM_PH_Broadleaf');settings=u.EditorScriptingMeshReductionOptions();settings.auto_compute_lod_screen_size=False
settings.reduction_settings=[u.EditorScriptingMeshReductionSettings(percent_triangles=p,screen_size=s) for p,s in [(1,1),(.4,.3),(.12,.12),(.03,.04)]]
sub.set_lods(tree,settings);assert tree.get_num_lods()==4;u.EditorAssetLibrary.save_loaded_asset(tree)
levels=u.get_editor_subsystem(u.LevelEditorSubsystem);levels.load_level(P+'/Greenlands_EnvironmentPreview')
for path in u.EditorAssetLibrary.list_assets(D+'/Foliage',recursive=False):
    ft=u.load_asset(path)
    if isinstance(ft,u.FoliageType_InstancedStaticMesh):ft.set_editor_property('cast_shadow',False);u.EditorAssetLibrary.save_loaded_asset(ft)
for actor in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors():
    if isinstance(actor,u.InstancedFoliageActor):
        for c in actor.get_components_by_class(u.InstancedStaticMeshComponent):
            c.set_editor_property('cast_shadow',False)
assert levels.save_current_level()
