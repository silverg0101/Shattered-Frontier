"""Retain centimetre texture precision along the kilometre-scale road UVs."""
import unreal as u
P='/Game/Frontier/Environment/GreenlandsPreview'
sub=u.get_editor_subsystem(u.StaticMeshEditorSubsystem);assert sub,'Use full editor -ExecutePythonScript'
mesh=u.load_asset(P+'/Arrival/Meshes/SM_GreenlandsWornRoads')
settings=sub.get_lod_build_settings(mesh,0);settings.set_editor_property('use_full_precision_u_vs',True);sub.set_lod_build_settings(mesh,0,settings)
assert u.EditorAssetLibrary.save_loaded_asset(mesh)
